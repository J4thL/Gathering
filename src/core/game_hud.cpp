// game_hud.cpp - biome atmosphere, prompts, minimap, the HUD (hotbar, bars) and the reticle.
#include <cstdio>
#include "core/game.hpp"
#include "core/logic_util.hpp"

// ================================================================= HUD
// ================================================================= biome atmosphere
void Game::drawBiomeFx() {
    float day = daylight();
    auto tint = [&](int b, int r, int g, int bl, int a) {
        if (bw[b] > 0.01f) fillRect(0, 0, LW, LH, RGBA(r, g, bl, (int)(a * bw[b])));
    };
    tint(B_DESERT, 255, 196, 90, (int)(30 * (0.35f + 0.65f * day)));  // hot haze, strongest at midday
    tint(B_SWAMP, 52, 86, 60, 46);                                    // green murk
    tint(B_SNOW, 200, 225, 255, (int)(30 * (0.5f + 0.5f * day)));     // cold blue light
    tint(B_MOUNTAIN, 150, 160, 190, 20);                              // thin cool air

    // ground fog / mist: world-anchored puffs so they drift with the map instead of the screen
    float fogK = bw[B_SWAMP] + bw[B_MOUNTAIN] * 0.30f + bw[B_SNOW] * 0.22f;
    if (fogK > 0.02f) {
        float tw = bw[B_SWAMP] + bw[B_MOUNTAIN] + bw[B_SNOW] + 1e-4f;
        int cr = (int)((170 * bw[B_SWAMP] + 215 * bw[B_MOUNTAIN] + 228 * bw[B_SNOW]) / tw);
        int cg = (int)((205 * bw[B_SWAMP] + 218 * bw[B_MOUNTAIN] + 238 * bw[B_SNOW]) / tw);
        int cb = (int)((170 * bw[B_SWAMP] + 232 * bw[B_MOUNTAIN] + 252 * bw[B_SNOW]) / tw);
        SDL_SetTextureColorMod(A.B.fog.t, (Uint8)cr, (Uint8)cg, (Uint8)cb);
        const int cell = 88;
        int gx0 = (camX - 60) / cell - 1, gy0 = (camY - 40) / cell - 1, gx1 = (camX + LW + 60) / cell + 1, gy1 = (camY + LH + 40) / cell + 1;
        for (int gy = gy0; gy <= gy1; gy++)
            for (int gx = gx0; gx <= gx1; gx++) {
                uint32_t h = hash2(gx, gy, 4242);
                // (int) below: added to an unsigned value a negative cell (the map's left / top edge) wrapped to ~4e9 and the puff was lost
                float px = gx * cell + (int)(h & 63) + sinf(timeAcc * 0.11f + (h >> 8 & 255) * 0.05f) * 16.f;
                float py = gy * cell + (int)((h >> 6) & 31) + cosf(timeAcc * 0.09f + (h >> 4 & 255) * 0.05f) * 6.f;
                float pulse = 0.65f + 0.35f * sinf(timeAcc * 0.25f + (h >> 12 & 255) * 0.1f);
                float a = clampf(fogK * pulse * (0.55f + 0.45f * (1.f - 0.5f * day)), 0, 1);
                SDL_SetTextureAlphaMod(A.B.fog.t, (Uint8)(a * 255));
                sprS(A.B.fog, (int)px - camX, (int)py - camY);
            }
        SDL_SetTextureAlphaMod(A.B.fog.t, 255);
    }

    // weather particles
    for (auto& f : flakes) {
        int x = (int)f.x, y = (int)f.y;
        if (bw[B_SNOW] > 0.02f) fillRect(x, y, f.sz > 0.7f ? 2 : 1, f.sz > 0.7f ? 2 : 1, RGBA(255, 255, 255, (int)(235 * bw[B_SNOW])));
        if (bw[B_DESERT] > 0.02f && f.sz > 0.35f) fillRect(x, y, 2, 1, RGBA(226, 196, 130, (int)(120 * bw[B_DESERT])));
        if (bw[B_SWAMP] > 0.02f && f.sz > 0.6f) fillRect(x, y, 1, 1, RGBA(46, 50, 30, (int)(190 * bw[B_SWAMP] * (0.4f + 0.6f * day))));
        if (bw[B_MOUNTAIN] > 0.02f && f.sz > 0.5f) fillRect(x, y, 2, 1, RGBA(225, 228, 238, (int)(100 * bw[B_MOUNTAIN])));
    }
}

void Game::drawBiomeBanner() {
    if (bannerT <= 0 || state != ST_PLAY || delve >= 0) return;  // delves have their own banner
    float k = std::min(1.f, (3.2f - bannerT) * 5.f);  // slide in
    float out = std::min(1.f, bannerT * 2.f);         // and fade out
    if (out < 0.35f) return;
    const BiomeDef& B = BIOME[bannerBiome];
    std::string name = bannerBiome == B_HOME ? "THE WILDWOOD" : B.name;
    int w = std::max(textW(name, 2), textW(bannerSub)) + 24, h = bannerSub.empty() ? 22 : 33;
    int x = LW / 2 - w / 2, y = 14 - (int)((1.f - k) * 40);
    panel(x, y, w, h, HEX(0x2b1d12));
    fillRect(x + 2, y + 2, w - 4, 2, B.col);
    textC(LW / 2, y + 8, name, B.col, 2);
    if (!bannerSub.empty()) textC(LW / 2, y + 22, bannerSub, C_DIM);
}

void Game::drawPrompt(float wx, float wy, const std::string& key, const std::string& label) {
    int sx = (int)wx - camX, sy = (int)wy - camY + (int)(sinf(timeAcc * 5.f) * 1.5f);
    int w = 20 + textW(label), h = 13;
    int x = sx - w / 2, y = sy - h;
    panel(x, y, w, h, HEX(0x3a2a1a));
    panel(x + 3, y + 2, 9, 9, HEX(0x92693a));
    textC(x + 7, y + 4, key, C_TEXT, 1, false);
    text(x + 16, y + 4, label, C_TEXT);
    fillRect(sx - 1, y + h, 3, 1, UI_BORDER);
    fillRect(sx, y + h + 1, 1, 1, UI_BORDER);
}

// "TO DESERT" over a gate when the hunter is close to it
void Game::drawPortalLabels() {
    for (int i = 0; i < MAPS[world.mapId].nPortals; i++) {
        const Portal& pt = MAPS[world.mapId].portals[i];
        const V2 c(((pt.zone.x0 + pt.zone.x1 + 1) * 0.5f) * TS, ((pt.zone.y0 + pt.zone.y1 + 1) * 0.5f) * TS);
        const float d = dist(p.pos, c);
        if (d > 130.f) continue;
        // sit the label inside the map, a few tiles in from the gap
        const V2 in = pt.side == 0 ? V2(-44, 0) : pt.side == 3 ? V2(44, 0) : pt.side == 1 ? V2(0, -34) : V2(0, 34);
        const int sx = (int)(c.x + in.x) - camX, sy = (int)(c.y + in.y) - camY;
        const std::string s = std::string("TO ") + MAPS[pt.toMap].shortName + (gateOpen[pt.toMap] ? "" : " (SEALED)");
        const int w = textW(s) + 8;
        panel(sx - w / 2, sy - 6, w, 12, HEX(0x2b1d12));
        textC(sx, sy - 2, s, BIOME[pt.toMap].col, 1, false);
    }
}

void Game::drawMinimap() {
    int x = LW - MM_W - 10, y = 6;
    panel(x, y, MM_W + 4, MM_H + 4, HEX(0x2b1d12));
    SDL_Rect d{x + 2, y + 2, MM_W, MM_H};
    SDL_SetTextureAlphaMod(miniTex, 235);
    SDL_RenderCopy(ren, miniTex, nullptr, &d);
    int ox = x + 2, oy = y + 2;
    // fog of war: cells the hunter has not been near yet are covered (row by row, runs of cells in one rectangle)
    if (seen.size() == (size_t)MM_W * MM_H)
        for (int cy = 0; cy < MM_H; cy++)
            for (int cx = 0; cx < MM_W;) {
                if (seen[(size_t)cy * MM_W + cx] == '1') { cx++; continue; }
                int e = cx;
                while (e < MM_W && seen[(size_t)cy * MM_W + e] == '0') e++;
                fillRect(ox + cx, oy + cy, e - cx, 1, HEX(0x1a1410));
                cx = e;
            }
    auto mx = [&](float wx) { return ox + (int)(wx / TS / MM_DIV); };
    auto my = [&](float wy) { return oy + (int)(wy / TS / MM_DIV); };
    // view rectangle
    u32 wc = RGBA(255, 255, 255, 120);
    int vx = mx((float)camX), vy = my((float)camY), vw = std::max(2, LW / TS / MM_DIV), vh = std::max(2, LH / TS / MM_DIV);
    fillRect(vx, vy, vw, 1, wc);
    fillRect(vx, vy + vh, vw, 1, wc);
    fillRect(vx, vy, 1, vh, wc);
    fillRect(vx + vw, vy, 1, vh + 1, wc);
    // shop and elder (home only), and the gates to the other maps in the colour of where they lead
    if (world.mapId == B_HOME) {
        fillRect(mx(world.hut.x) - 1, my(world.hut.y) - 1, 3, 3, C_GOLD);
        fillRect(mx(world.questGiver.x), my(world.questGiver.y), 2, 2, C_BLUE);
    }
    for (int i = 0; i < MAPS[world.mapId].nPortals; i++) {
        const Portal& pt = MAPS[world.mapId].portals[i];
        fillRect(ox + (pt.zone.x0 + pt.zone.x1) / 2 - 1, oy + (pt.zone.y0 + pt.zone.y1) / 2 - 1, 3, 3, BIOME[pt.toMap].col);
    }
    // discovered landmarks, in their biome's colour
    for (auto& l : world.landmarks)
        if (l.found) fillRect(mx(l.x) - 1, my(l.y) - 1, 2, 2, BIOME[l.biome].col);
    // carcasses that still hold materials
    for (auto& a : animals)
        if (a.dead && a.totalRemain > 0) fillRect(mx(a.pos.x), my(a.pos.y), 2, 2, C_RED);
    // player
    if (((int)(timeAcc * 3)) & 1 || state != ST_PLAY) fillRect(mx(p.pos.x) - 1, my(p.pos.y) - 1, 3, 3, HEX(0xffffff));
    fillRect(mx(p.pos.x), my(p.pos.y), 1, 1, HEX(0x2d5e35));
    // where am I
    const BiomeDef& B = BIOME[curBiome];
    text(x + 2, y + MM_H + 8, curBiome == B_HOME ? "WILDWOOD" : B.name, B.col);
    if (B.climate[0]) text(x + 2, y + MM_H + 15, B.climate, C_DIM);
    text(x + MM_W - textW(S(landmarksFound()) + "/" + S((int)world.landmarks.size())) + 2, y + MM_H + 8, S(landmarksFound()) + "/" + S((int)world.landmarks.size()), C_DIM);
}

void Game::drawHud() {
    if (state == ST_TITLE) return;
    // ---- floating marker over the elder: "!" = quest available, "?" = ready to turn in
    if (!questActive || questReady()) {
        int qx = (int)world.questGiver.x - camX, qy = (int)world.questGiver.y - camY - 38 + (int)(sinf(timeAcc * 4.f) * 1.5f);
        if (qx > -10 && qx < LW + 10 && qy > -12 && qy < LH + 12)
            text(qx - 3, qy, questActive ? "?" : "!", questActive ? C_GREEN : C_GOLD, 2);
    }
    // ---- context prompts in the world
    if (state == ST_PLAY) drawSpeechBubbles();
    if (state == ST_PLAY && !p.dead) {
        Animal* c = nearestCarcass(26);
        if (c) {
            int sx = (int)c->pos.x - camX, sy = (int)c->pos.y - camY;
            // remaining materials
            int n = 0;
            for (int m = 0; m < M_COUNT; m++)
                if (c->remain[m] > 0) n++;
            int pw = n * 20 + 6;
            int px = sx - pw / 2, py = sy - 46;
            panel(px, py, pw, 21, HEX(0x3a2a1a));
            int cx = px + 3;
            for (int m = 0; m < M_COUNT; m++) {
                if (c->remain[m] <= 0) continue;
                icon(A.matIcon[m], cx + 8, py + 8, 14);
                text(cx + 12, py + 13, S(c->remain[m]), C_TEXT);
                cx += 20;
            }
            if (p.gathering) {
                float f = p.gatherT / ANIMAL[c->kind].gatherTime;
                bar(sx - 14, sy - 22, 28, 4, f, C_GREEN);
            } else
                drawPrompt(c->pos.x, c->pos.y - 22, "E", "HOLD TO GATHER");
        } else if (structNear(24.f)) {
            drawStructPrompt();
        } else {
            HTarget ht;
            if (hsOutranksHarvest()) homesteadPrompt();
            else if (findHarvest(ht)) drawHarvestPrompt();
            else if (homesteadPrompt()) {}
            else if (townPrompt()) {}
            else if (delvePrompt()) {}
            else if (nearQuestGiver()) drawPrompt(world.questGiver.x, world.questGiver.y - 44, "E", "TALK");
        }
    }

    // ---- low health: the screen edges pulse red (and faster the lower it gets); nothing is drawn above 30%
    if (state == ST_PLAY && !p.dead && p.hp < p.maxhp * 0.3f) {
        const float low = 1.f - p.hp / (p.maxhp * 0.3f);  // 0 at the threshold .. 1 at no health
        const float beat = 0.5f + 0.5f * sinf(timeAcc * (4.f + 5.f * low));
        const int a = (int)(25 + (35 + 60 * low) * beat);
        for (int i = 0; i < 4; i++) {  // four soft layers, thicker towards the edge
            const int t = 2 + i * 2;
            const u32 c = RGBA(200, 30, 30, a / (1 + i));
            fillRect(0, 0, LW, t, c);
            fillRect(0, LH - t, LW, t, c);
            fillRect(0, 0, t, LH, c);
            fillRect(LW - t, 0, t, LH, c);
        }
    }
    // ---- top-left vitals
    panel(4, 4, 98, 40, HEX(0x33241a));
    icon(A.heart, 12, 12, 11);
    float hf = p.hp / p.maxhp;
    bar(21, 9, 74, 7, hf, hf > 0.5f ? HEX(0x59b543) : hf > 0.25f ? HEX(0xe0a93a) : HEX(0xd8443f));
    text(23, 10, S((int)ceilf(p.hp)) + "/" + S((int)p.maxhp), C_TEXT, 1, true);
    icon(A.bolt, 12, 24, 9);
    bar(21, 21, 74, 4, p.stamina / 100.f, p.exhausted ? HEX(0xa86a2a) : HEX(0xe0c43a));
    icon(A.coin, 12, 35, 10);
    text(21, 33, S(p.coins), C_GOLD);

    // ---- clock
    {
        float t = tod();
        int mins = (int)(fmodf(6.f + t * 24.f, 24.f) * 60);
        char buf[32];
        snprintf(buf, sizeof(buf), "DAY %d  %02d:%02d", 1 + (int)(dayT / 300.f), mins / 60, mins % 60);
        int w = textW(buf) + 18;
        int x = LW / 2 - w / 2 - 8;
        panel(x, 4, w, 13, HEX(0x33241a));
        bool dayT_ = daylight() > 0.4f;
        fillRect(x + 5, 8, 5, 5, dayT_ ? C_GOLD : HEX(0xcfd8e8));
        if (!dayT_) fillRect(x + 7, 8, 3, 3, HEX(0x33241a));
        text(x + 13, 8, buf, C_TEXT);
    }
    // ---- messages
    {
        int y = 21;
        for (auto& m : msgs) {
            float a = std::min(1.f, m.t / 0.6f);
            textC(LW / 2 - 8, y, m.n > 1 ? m.s + " X" + S(m.n) : m.s, m.col, 1, true);
            (void)a;
            y += 8;
        }
    }
    if (showMap && interior < 0 && delve < 0) drawMinimap();
    drawBiomeBanner();
    if (state == ST_PLAY && interior < 0 && delve < 0) drawPortalLabels();
    if (delve >= 0) { drawDelveBanner(); drawBossBar(); }

    // ---- timed effects under the vitals
    {
        int ty = 46;
        if (p.warmT > 0) text(6, ty, "WARM BREW " + S((int)p.warmT) + "S", C_ORANGE), ty += 8;
        if (p.coolT > 0) text(6, ty, "REFRESHED " + S((int)p.coolT) + "S", C_BLUE), ty += 8;
        if (warmthAt(p.pos) > 0.35f) text(6, ty, "BY THE FIRE", C_ORANGE), ty += 8;
    }
    // ---- weight meter (the material counters are gone from the screen: materials live in the pack, press I)
    const int stripY = LH - 4;
    // ---- active quest tracker (bottom-left, above the weight meter)
    if (questActive) {
        bool ready = questReady();
        std::string t = ready ? quest.title + "  DONE! SEE ELDER" : quest.title + "  " + S(questProg) + "/" + S(quest.goal);
        panel(4, stripY - 22, textW(t) + 8, 12, HEX(0x33241a));
        text(8, stripY - 19, t, ready ? C_GREEN : C_TEXT, 1, false);
    }
    {
        const float tw = p.inv.totalWeight(), mw = p.inv.maxWeight;
        const float fr = mw > 0 ? std::min(1.f, tw / mw) : 1.f;
        const bool over = tw > mw + 1e-3f;
        const u32 col = over ? C_RED : fr >= 0.85f ? C_ORANGE : C_TEXT;
        text(5, stripY - 9, "CARRY " + kgNum(tw) + "/" + kgNum(mw) + " KG" + (over ? "  SLOW" : ""), col);
        bar(5, stripY - 1, 72, 2, fr, over ? HEX(0xd8443f) : fr >= 0.85f ? HEX(0xe88a3a) : HEX(0x8ec2f0));
    }
    // ---- a farm tool in hand: outline the tile under the cursor (green = in reach, red = too far)
    if (p.hand >= 0 && inHomestead() && interior < 0 && !p.dead && state == ST_PLAY) {
        const int tx = (int)floorf((in.mx + camX) / (float)TS), ty = (int)floorf((in.my + camY) / (float)TS);
        const int sx = tx * TS - camX, sy = ty * TS - camY;
        const u32 c = dist(p.pos, V2((tx + .5f) * TS, (ty + .5f) * TS)) <= 46.f ? RGBA(120, 230, 120, 200) : RGBA(230, 90, 80, 200);
        fillRect(sx, sy, TS, 1, c);
        fillRect(sx, sy + TS - 1, TS, 1, c);
        fillRect(sx, sy, 1, TS, c);
        fillRect(sx + TS - 1, sy, 1, TS, c);
    }
    // ---- hotbar: only what the hunter has equipped; empty slots stay empty
    {
        constexpr int sw = 22, gap = 3;
        const int total = HOT_N * sw + (HOT_N - 1) * gap, x0 = (LW - total) / 2;
        const int y = LH - sw - 5;
        for (int i = 0; i < HOT_N; i++) {
            const int x = x0 + i * (sw + gap);
            const int id = p.hot[i];
            const bool isWpn = id >= 0 && (item(id).kind == IK_THROWN || item(id).kind == IK_LAUNCHER);
            const bool isTool = id >= 0 && item(id).kind == IK_TOOL;
            const bool sel = p.hand >= 0 ? i == p.hotSel : (isWpn && item(id).ref == p.sel);  // the slot in hand
            const int yy = y - (sel ? 3 : 0);
            panel(x, yy, sw, sw, sel ? HEX(0x6a4a2a) : HEX(0x33241a));
            if (sel) {
                fillRect(x, yy + 1, 1, sw - 2, C_GOLD);
                fillRect(x + sw - 1, yy + 1, 1, sw - 2, C_GOLD);
                fillRect(x + 1, yy, sw - 2, 1, C_GOLD);
                fillRect(x + 1, yy + sw - 1, sw - 2, 1, C_GOLD);
            }
            text(x + 2, yy + 2, S(i + 1), C_DIM, 1, false);
            if (id < 0) continue;
            // launchers show how much ammunition is left instead of how many you own
            const int shown = isWpn && item(id).kind == IK_LAUNCHER ? p.ammo(WPN[item(id).ref].ammo) : p.inv.count(id);
            const bool have = isWpn && item(id).kind == IK_LAUNCHER ? (p.inv.count(id) > 0 && shown > 0) : shown > 0;
            if (have && itemHasQuality(id)) qualityFrame(x + 1, yy + 1, sw - 2, sw - 2, p.inv.bestQuality(id));
            icon(itemIcon(id), x + sw / 2, yy + sw / 2, 16, have ? 1.f : 0.3f);
            if (!isTool) {  // (a tool is one item: no count)
                const std::string n = S(shown);
                text(x + sw - 3 - textW(n), yy + sw - 8, n, have ? C_TEXT : C_RED);
            }
            if (sel && p.cd > 0) bar(x + 3, yy + sw - 3, sw - 6, 1, 1.f - p.cd / WPN[item(id).ref].cd, C_GOLD);
        }
        if (p.hand >= 0 && p.hotSel >= 0 && p.hot[p.hotSel] >= 0) {
            const int hid = p.hot[p.hotSel];
            const char* hint = seedCropOf(hid) >= 0 ? " - CLICK TILLED SOIL" : item(hid).kind == IK_TOOL ? " - CLICK A TILE" : item(hid).kind == IK_CONSUMABLE ? " - CLICK TO USE" : item(hid).kind == IK_PLACEABLE ? " - CLICK TO PLACE" : item(hid).kind == IK_BACKPACK ? " - CLICK TO WEAR" : "";
            textC(x0 + total / 2, y - 12, item(hid).name + hint, C_TEXT);
        }
        else if (p.equippedWpn(p.sel)) textC(x0 + total / 2, y - 12, WPN[p.sel].name, C_TEXT);
    }
    // ---- help card
    if (helpT > 0 && state == ST_PLAY) {
        const std::string lines[] = {"MOVE " + keyName(K_UP) + keyName(K_LEFT) + keyName(K_DOWN) + keyName(K_RIGHT) + "   " + keyName(K_SPRINT) + " SPRINT   1-8 HOTBAR   " + keyName(K_HEAL) + " HEAL",
                                     "HOLD LEFT CLICK TO CHARGE, RELEASE TO THROW",
                                     "HOLD " + keyName(K_USE) + " AT A CARCASS, TREE, ROCK OR PLANT", "AXE FOR TREES, PICKAXE FOR ORE (CRAFT IN PACK)",
                                     keyName(K_PACK) + " PACK  " + keyName(K_MAP) + " MAP  TAB MINIMAP  " + keyName(K_HELP) + " HELP  WHEEL: HOTBAR  ESC PAUSE",
                                     "PACK: DRAG TO MOVE OR EQUIP, RIGHT CLICK TO USE, SLIDER + SPLIT / DROP"};
        int tw = 0;  // the panel is as wide as its longest line plus a margin, so no line can overflow it
        for (const std::string& l : lines) tw = std::max(tw, textW(l));
        const int w = tw + 20, h = 59;
        const int x = LW / 2 - w / 2, y = 52;
        panel(x, y, w, h, HEX(0x33241a));
        textC(LW / 2, y + 5, "HOW TO HUNT", C_GOLD);
        for (int i = 0; i < 6; i++) textC(LW / 2, y + 14 + i * 7, lines[i], C_TEXT, 1, false);
    }
    // ---- knocked out
    if (p.dead) {
        float a = clampf(p.deadT / 0.8f, 0, 1);
        fillRect(0, 0, LW, LH, RGBA(10, 4, 4, (int)(a * 170)));
        textC(LW / 2, LH / 2 - 14, "KNOCKED OUT", C_RED, 3);
        textC(LW / 2, LH / 2 + 8, "WAKING UP BY THE CAMPFIRE...", C_TEXT);
    }
}

void Game::drawReticle() {
    int mx = (int)in.mx, my = (int)in.my;
    bool over = false;
    for (auto& a : animals)
        if (!a.dead && dist(V2(in.mx + camX, in.my + camY), a.pos + V2(0, -a.bodyH - a.air)) < ANIMAL[a.kind].hitR * a.scale + 4) over = true;
    u32 col = over ? HEX(0xff6a5a) : HEX(0xffffff);
    u32 sh = HEX(0x140d07);
    // aim line while charging
    if (p.charging) {
        V2 hand = p.pos + V2(0, -10);
        V2 mw(in.mx + camX, in.my + camY);
        V2 d = mw - hand;
        float dl = d.len();
        if (dl > 6) {
            V2 dir = d * (1.f / dl);
            const WpnDef& W = WPN[p.sel];
            float range = isLauncher(p.sel) ? std::min(W.range * (0.5f + 0.5f * (W.draw > 0 ? p.charge : 1.f)), dl + 24)
                                            : std::min(W.range * (1 + 0.15f * p.charge), dl + 24);
            for (float t = 14; t < range; t += 6) {
                V2 q = hand + dir * t;
                fillRect((int)q.x - camX, (int)q.y - camY, 1, 1, RGBA(255, 255, 255, (int)(190 * (1.f - t / (range + 20)))));
            }
            V2 e = hand + dir * range;
            int ex = (int)e.x - camX, ey = (int)e.y - camY;
            fillRect(ex - 2, ey, 5, 1, RGBA(255, 220, 120, 220));
            fillRect(ex, ey - 2, 1, 5, RGBA(255, 220, 120, 220));
        }
    }
    auto tick = [&](int x, int y, int w, int h) {
        fillRect(x - 1, y - 1, w + 2, h + 2, sh);
        fillRect(x, y, w, h, col);
    };
    int g = p.charging ? 4 - (int)(p.charge * 2) : 5;
    tick(mx - g - 3, my, 3, 1);
    tick(mx + g + 1, my, 3, 1);
    tick(mx, my - g - 3, 1, 3);
    tick(mx, my + g + 1, 1, 3);
    fillRect(mx, my, 1, 1, col);
    if (p.charging) {
        int n = (int)(p.charge * 24);
        for (int i = 0; i < n; i++) {
            float a = -1.5708f + i * 6.2832f / 24.f;
            fillRect(mx + (int)roundf(cosf(a) * 10) , my + (int)roundf(sinf(a) * 10), 1, 1, p.charge >= 1 ? C_GOLD : HEX(0xffffff));
        }
    }
}
