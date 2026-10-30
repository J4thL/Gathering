// main.cpp - entry point, input and fixed-timestep loop.
// Test flags:  --headless (dummy video/audio)  --shots DIR (write screenshots then exit)
//              --worldcheck (reachability + walking test)  --tour DIR (screenshots of every map)  --dumpcore FILE (home-area dump)
#include <map>
#include <set>
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdlib>
#include <queue>
#include <algorithm>
#include <fstream>
#include "game.hpp"

#ifdef _WIN32
static int setenv(const char* k, const char* v, int) { return _putenv_s(k, v); }
#endif

static void mapMouse(Game& g, int wx, int wy) {
    g.in.mx = (wx - g.offX) / g.presScale;
    g.in.my = (wy - g.offY) / g.presScale;
}

static void pollInput(Game& g) {
    Input& in = g.in;
    // Edge flags (pressed/released/toggles) are cleared in main() after update() has
    // consumed them, so a frame that runs no update step can't swallow an input.
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT: g.running = false; break;
            case SDL_MOUSEMOTION: mapMouse(g, e.motion.x, e.motion.y); break;
            case SDL_MOUSEBUTTONDOWN:
                mapMouse(g, e.button.x, e.button.y);
                if (e.button.button == SDL_BUTTON_LEFT) {
                    in.l = true;
                    in.lPressed = true;
                    in.uiClick = true;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                mapMouse(g, e.button.x, e.button.y);
                if (e.button.button == SDL_BUTTON_LEFT) {
                    in.l = false;
                    in.lReleased = true;
                }
                break;
            case SDL_MOUSEWHEEL: in.wheel += e.wheel.y; break;
            case SDL_KEYDOWN:
                if (e.key.repeat) break;
                switch (e.key.keysym.sym) {
                    case SDLK_e: in.ePressed = true; break;
                    case SDLK_ESCAPE: in.escPressed = true; break;
                    case SDLK_TAB: in.tabPressed = true; break;
                    case SDLK_RETURN: case SDLK_SPACE: in.enterPressed = true; break;
                    case SDLK_q: in.bandage = true; break;
                    case SDLK_m: in.mapToggle = true; break;
                    case SDLK_h: in.helpToggle = true; break;
                    case SDLK_i: in.invToggle = true; break;
                    case SDLK_1: in.selectKey = 0; break;
                    case SDLK_2: in.selectKey = 1; break;
                    case SDLK_3: in.selectKey = 2; break;
                    case SDLK_4: in.selectKey = 3; break;
                    case SDLK_5: in.selectKey = 4; break;
                    case SDLK_6: in.selectKey = 5; break;
                    case SDLK_7: in.selectKey = 6; break;
                    case SDLK_8: in.selectKey = 7; break;
                    case SDLK_9: in.selectKey = 8; break;
                    case SDLK_0: in.selectKey = 9; break;
                    case SDLK_F11:
                        g.fullscreen = !g.fullscreen;
                        SDL_SetWindowFullscreen(g.win, g.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                        break;
                    default: break;
                }
                break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) in.l = false;
                break;
        }
    }
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    in.up = k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP];
    in.down = k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN];
    in.left = k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT];
    in.right = k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT];
    in.sprint = k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT];
    in.e = k[SDL_SCANCODE_E];
}

// ---- scripted run used for automated screenshots / smoke testing
static void scriptedRun(Game& g, const std::string& dir) {
    auto frames = [&](int n, float dt = 1.f / 60.f) {
        for (int i = 0; i < n; i++) {
            g.update(dt);
            g.in.lPressed = g.in.lReleased = g.in.ePressed = g.in.escPressed = g.in.enterPressed = g.in.tabPressed = false;
            g.in.selectKey = -1;
            g.in.mapToggle = g.in.helpToggle = g.in.invToggle = false;
        }
        g.render();
    };
    auto shot = [&](const char* name) { g.saveCanvas((dir + "/" + name).c_str()); };

    frames(120);
    shot("01_title.bmp");

    g.newGame();
    g.state = ST_PLAY;
    g.p.coins = 250;
    for (int m = 0; m < M_CARCASS_N; m++) g.p.inv.set(itemMat(m), 4 + m);
    frames(30);
    shot("02_village.bmp");
    g.helpT = 0;
    for (int m = 0; m < M_COUNT; m++) g.p.inv.set(itemMat(m), 0);

    // stalk and kill the nearest deer, throwing from range
    Animal* tgt = nullptr;
    float best = 1e9f;
    for (auto& a : g.animals)
        if (a.kind == A_DEER && dist(a.pos, g.p.pos) < best) {
            best = dist(a.pos, g.p.pos);
            tgt = &a;
        }
    if (tgt) {
        tgt->pos = g.p.pos + V2(90, -40);
        tgt->st = AS_WANDER;
        tgt->idleT = 99;
        g.p.inv.set(itemWpn(W_JAVELIN), 6);
        g.p.sel = W_JAVELIN;
        frames(5);
        int throws = 0;
        for (int tries = 0; tries < 400 && tgt && !tgt->dead; tries++) {
            // aim at the deer's centre, hold to charge then release
            g.in.mx = tgt->pos.x - g.camX;
            g.in.my = tgt->pos.y - tgt->bodyH - g.camY;
            g.in.l = true;
            g.in.lPressed = (g.p.charge == 0 && !g.p.charging);
            frames(1);
            if (tries == 12) shot("03_charging.bmp");
            if (g.p.charge >= 0.6f) {
                g.in.l = false;
                g.in.lReleased = true;
                frames(1);
                throws++;
                for (int w = 0; w < 14; w++) {
                    frames(1);
                    if (w == 4 && throws == 1) shot("04_flight.bmp");
                }
                tgt->st = AS_WANDER;
                tgt->idleT = 99;
                tgt->kb = V2();
            }
        }
        printf("deer killed after %d throws: %s\n", throws, tgt && tgt->dead ? "yes" : "no");
        frames(20);
        shot("05_kill.bmp");
        // walk to the carcass and gather
        if (tgt && tgt->dead) {
            g.p.pos = tgt->pos + V2(-12, 6);
            g.p.inv.set(itemMat(M_MEAT), 0);
            g.in.mx = 200;
            g.in.my = 112;
            g.in.e = true;
            for (int i = 0; i < 80; i++) frames(1);
            shot("06_gather.bmp");
            g.in.e = false;
            for (int i = 0; i < 420 && tgt->totalRemain > 0; i++) {
                g.in.e = true;
                frames(1);
            }
            g.in.e = false;
            printf("carcass remaining: %d  bag: %d/%d\n", tgt->totalRemain, g.p.bagUsed(), g.p.bagCap());
        }
    }

    // shop screens
    g.p.pos = g.world.keeper + V2(-8, 14);
    frames(10);
    g.in.ePressed = true;
    g.update(1.f / 60.f);
    g.in.ePressed = false;
    printf("state after E at shop: %d (2 = shop)\n", (int)g.state);
    for (int t = 0; t < 4; t++) {
        g.shopTab = t;
        g.in.mx = 200;
        g.in.my = 100;
        frames(2);
        char nm[64];
        snprintf(nm, sizeof(nm), "07_shop_%d.bmp", t);
        shot(nm);
    }
    // exercise the economy
    int before = g.p.coins;
    g.sellMat(M_MEAT, 99);
    g.sellMat(M_HIDE, 99);
    printf("sold: coins %d -> %d\n", before, g.p.coins);
    g.p.inv.set(itemMat(M_BONE), 10);
    g.p.inv.set(itemMat(M_HIDE), 10);
    g.p.inv.set(itemMat(M_TUSK), 5);
    g.p.inv.set(itemMat(M_FUR), 5);
    g.p.inv.set(itemMat(M_ANTLER), 5);
    bool c1 = g.craft(0), c2 = g.craft(1), c3 = g.craft(2);
    printf("crafted bone/axe/javelin: %d %d %d\n", c1, c2, c3);
    g.p.coins = 1000;
    g.p.inv.set(itemMat(M_BONE), 20);
    g.p.inv.set(itemMat(M_HIDE), 20);
    g.p.inv.set(itemMat(M_TUSK), 20);
    g.p.inv.set(itemMat(M_FUR), 20);
    bool u = g.buyUpgrade(U_TOOL);
    printf("upgrade tool: %d (lvl %d)\n", u, g.p.up[U_TOOL]);
    g.shopTab = 2;
    frames(2);
    shot("08_upgrades.bmp");
    g.state = ST_PLAY;

    // gathering and crafting screens
    {
        g.p.up[U_BAG] = 2;
        g.p.syncPack();
        for (int m = M_WOOD; m <= M_GOLDBAR; m++) g.p.inv.set(itemMat(m), 3 + m % 5);
        g.p.inv.add(itemTool(TL_AXE2), 1);
        g.p.inv.add(itemTool(TL_PICK1), 1);
        g.p.inv.add(itemPlace(PL_CAMPFIRE), 1);
        g.p.inv.add(itemCons(C_SALVE), 2);
        g.p.coins = 250;
        g.state = ST_INVENTORY;
        g.invTab = 0;
        frames(3);
        shot("20_pack.bmp");
        g.invTab = 1;
        g.craftCat = -1;
        frames(3);
        shot("21_craft_all.bmp");
        g.craftOnly = true;
        frames(3);
        shot("22_craft_canmake.bmp");
        g.p.sk.s[SK_HUNTING].xp = 900;  // a few trained skills so the SKILLS page shows bars, a level-up row and a capped one
        g.p.sk.s[SK_WOODCUTTING].xp = 55;
        g.p.sk.s[SK_SKINNING].xp = 12000;
        g.p.sk.s[SK_RANGED].xp = SkillState::capXp();
        g.invTab = 2;
        g.in.mx = 24 + 20;
        g.in.my = 12 + 38 + 3 * 9 + 4;  // hover the HUNTING row: footer shows its next milestone
        frames(3);
        shot("23_skills.bmp");
        g.invTab = 0;
        g.p.sk = SkillSet();
        g.craftOnly = false;
        g.state = ST_PLAY;
        // standing at an oak with an axe, mid-swing
        for (size_t i = 0; i < g.world.props.size(); i++)
            if (g.propRsId[i] == RS_OAK && dist(V2(g.world.props[i].x, g.world.props[i].y), g.world.spawn) > 150) {
                g.p.pos = V2(g.world.props[i].x, g.world.props[i].y + 12);
                break;
            }
        g.camX = (int)g.p.pos.x - LW / 2;
        g.in.e = true;
        frames(40);
        shot("23_chopping.bmp");
        frames(400);
        g.in.e = false;
        shot("24_felled.bmp");
    }

    // night + wolf + hud
    g.p.pos = g.world.village + V2(0, 70);
    g.dayT = 300.f * 0.55f;
    g.showMap = true;
    frames(40);
    shot("09_night.bmp");
    g.state = ST_PAUSE;
    frames(2);
    shot("10_pause.bmp");
    printf("scripted run complete; animals=%zu\n", g.animals.size());
}


// ---- world verification (flags: --worldcheck, --dumpcore FILE, --tour DIR)
// Dump of the original home area's interior (tiles, solid flags, props, decor). Written by the old generator
// and the new one in the same format so the two can be diffed: the start area must be unchanged.
static void dumpCore(Game& g, const char* file) {
    const World& w = g.world;
    const int X0 = 16, X1 = 74, Y0 = 4, Y1 = 56;  // interior, clear of the gates cut into the tree wall
    std::ofstream f(file);
    for (int y = Y0; y < Y1; y++) {
        for (int x = X0; x < X1; x++) f << (char)('a' + w.tile[COY + y][COX + x]) << (w.solid[COY + y][COX + x] ? '#' : '.');
        f << "\n";
    }
    std::vector<std::string> lines;
    char buf[96];
    for (auto& p : w.props) {
        float x = p.x - COX * TS, y = p.y - COY * TS;
        if (x < X0 * TS || x >= X1 * TS || y < Y0 * TS || y >= Y1 * TS) continue;
        snprintf(buf, sizeof(buf), "P %d %.2f %.2f", p.kind, x, y);
        lines.push_back(buf);
    }
    for (auto& d : w.decor) {
        float x = d.x - COX * TS, y = d.y - COY * TS;
        if (x < X0 * TS || x >= X1 * TS || y < Y0 * TS || y >= Y1 * TS) continue;
        snprintf(buf, sizeof(buf), "D %d %.2f %.2f", d.kind, x, y);
        lines.push_back(buf);
    }
    std::sort(lines.begin(), lines.end());
    for (auto& l : lines) f << l << "\n";
}

struct Reach {
    static constexpr int CELL = 8;
    int gw, gh;
    std::vector<int> d;  // distance in cells from the start, -1 = unreachable
    Reach(const World& w, V2 start, float rad = 4.f) {
        gw = (int)(w.pixW() / CELL);
        gh = (int)(w.pixH() / CELL);
        d.assign((size_t)gw * gh, -1);
        auto pass = [&](int cx, int cy) { return cx >= 0 && cy >= 0 && cx < gw && cy < gh && !w.blocked(cx * CELL + CELL / 2.f, cy * CELL + CELL / 2.f, rad); };
        int sx = (int)(start.x / CELL), sy = (int)(start.y / CELL);
        if (!pass(sx, sy)) return;
        std::queue<std::pair<int, int>> q;
        q.push({sx, sy});
        d[(size_t)sy * gw + sx] = 0;
        while (!q.empty()) {
            auto [x, y] = q.front();
            q.pop();
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (!dx && !dy) continue;
                    int nx = x + dx, ny = y + dy;
                    if (!pass(nx, ny) || d[(size_t)ny * gw + nx] >= 0) continue;
                    if (dx && dy && (!pass(x + dx, y) || !pass(x, y + dy))) continue;  // no corner cutting
                    d[(size_t)ny * gw + nx] = d[(size_t)y * gw + x] + 1;
                    q.push({nx, ny});
                }
        }
    }
    int at(float px, float py) const {
        int cx = (int)(px / CELL), cy = (int)(py / CELL);
        if (cx < 0 || cy < 0 || cx >= gw || cy >= gh) return -1;
        return d[(size_t)cy * gw + cx];
    }
    bool nearReach(float px, float py, int rad = 3) const {
        for (int dy = -rad; dy <= rad; dy++)
            for (int dx = -rad; dx <= rad; dx++)
                if (at(px + dx * CELL, py + dy * CELL) >= 0) return true;
        return false;
    }
};

// start a fresh game and stand at the entrance of map `b` (the home map is where a new game starts)
static void enterMap(Game& g, int b) {
    g.newGame();
    g.state = ST_PLAY;
    g.helpT = 0;
    if (b != B_HOME) g.travelTo(b, MAPS[b].entryTx, MAPS[b].entryTy, false);
    g.travelCd = g.travelFade = 0;
    g.animals.clear();
    g.respawnT = 1e9f;
}

// drive the real player through update() along the shortest route to `target` (pixels) in the active map, until the map changes or
// the target is reached. Returns the number of frames used (-1 = never arrived).
static int driveTo(Game& g, V2 target, int stopIfMapChanges, int maxFrames = 60 * 240) {
    const World& w = g.world;
    Reach back(w, target, 6.f);  // planned with a fatter body than the real one (4px) so steering can't clip corners
    const int startMap = w.mapId;
    for (int frames = 0; frames < maxFrames; frames++) {
        int cx = (int)(g.p.pos.x / 8), cy = (int)(g.p.pos.y / 8);
        int cur = back.at(g.p.pos.x, g.p.pos.y);
        if (cur >= 0 && cur <= 2 && !stopIfMapChanges) return frames;
        int bx = cx, by = cy, bd = cur < 0 ? 1 << 30 : cur;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int nx = cx + dx, ny = cy + dy;
                if (nx < 0 || ny < 0 || nx >= back.gw || ny >= back.gh) continue;
                int nd = back.d[(size_t)ny * back.gw + nx];
                if (dx && dy && (back.d[(size_t)cy * back.gw + cx + dx] < 0 || back.d[(size_t)(cy + dy) * back.gw + cx] < 0)) continue;  // no corner cutting
                if (nd >= 0 && nd < bd) bd = nd, bx = nx, by = ny;
            }
        V2 tgt(bx * 8.f + 4, by * 8.f + 4), dv = tgt - g.p.pos;
        g.in.left = dv.x < -0.3f, g.in.right = dv.x > 0.3f, g.in.up = dv.y < -0.3f, g.in.down = dv.y > 0.3f;
        g.in.sprint = false;
        g.p.stamina = 100;
        g.p.hp = g.p.maxhp;  // this test is about terrain, not about wolves
        g.animals.clear();
        g.respawnT = 1e9f;
        g.update(1.f / 60.f);
        if (g.world.mapId != startMap) {
            g.in.left = g.in.right = g.in.up = g.in.down = false;
            return frames;
        }
    }
    g.in.left = g.in.right = g.in.up = g.in.down = false;
    return -1;
}

// the open tile of a gate's zone nearest its middle, as a pixel position (where a walker aims to go through the gate)
static V2 portalPoint(const World& w, const Portal& pt) {
    const float mx = (pt.zone.x0 + pt.zone.x1) * 0.5f, my = (pt.zone.y0 + pt.zone.y1) * 0.5f;
    V2 best((mx + 0.5f) * TS, (my + 0.5f) * TS);
    float bd = 1e9f;
    for (int y = pt.zone.y0; y <= pt.zone.y1; y++)
        for (int x = pt.zone.x0; x <= pt.zone.x1; x++)
            if (!w.solidTile(x, y) && hypotf(x - mx, y - my) < bd) bd = hypotf(x - mx, y - my), best = V2(x * TS + 8.f, y * TS + 8.f);
    return best;
}

// Checks one map: size, biome purity, every landmark / resource / hub / gate reachable from where you arrive, edge wall closed except at the gates.
static int mapCheck(Game& g, int map, long nodeTotals[]) {
    int bad = 0;
    enterMap(g, map);
    World& w = g.world;
    const MapDef& MD = MAPS[map];
    const V2 arrive = g.p.pos;
    Reach rc(w, arrive);
    printf("%s map: %dx%d tiles (%dx%d px), mapId %d, props=%zu decor=%zu nodes=%zu landmarks=%zu\n", MD.name, MW, MH, (int)w.pixW(), (int)w.pixH(), w.mapId, w.props.size(), w.decor.size(), w.nodes.size(), w.landmarks.size());
    if (MW != 240 || MH != 168 || w.mapId != map) {
        printf("  PROBLEM: map size is not 240x168\n");
        bad++;
    }
    // purity: every tile belongs to this map's biome; the home map has only its own four ground types
    long foreign = 0, wrongGround = 0;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            if (w.biome[y][x] != map) foreign++;
            const uint8_t t = w.tile[y][x];
            if (map == B_HOME && !(t == T_GRASS || t == T_DARK || t == T_DIRT || t == T_WATER)) wrongGround++;
        }
    printf("  tiles of another biome: %ld%s   foreign ground in home: %ld\n", foreign, foreign ? "  <-- PROBLEM" : "", wrongGround);
    if (foreign || wrongGround) bad++;
    // reach
    long total = 0, reach = 0;
    for (int cy = 0; cy < rc.gh; cy++)
        for (int cx = 0; cx < rc.gw; cx++)
            if (!w.blocked(cx * 8.f + 4, cy * 8.f + 4, 4.f)) {
                total++;
                if (rc.d[(size_t)cy * rc.gw + cx] >= 0) reach++;
            }
    printf("  walkable cells %ld, reachable from the arrival point %ld (%.1f%%)\n", total, reach, total ? 100.0 * reach / total : 0.0);
    {
        const int d = rc.at(w.hub[map].x, w.hub[map].y);
        printf("  hub reachable: %s  path length %d px\n", d >= 0 ? "yes" : "NO", d >= 0 ? d * Reach::CELL : -1);
        if (d < 0) bad++;
    }
    for (auto& l : w.landmarks) {
        bool ok = rc.nearReach(l.x, l.y, 2);
        printf("  landmark %-18s reachable: %s\n", l.name, ok ? "yes" : "NO");
        if (!ok) bad++;
    }
    int nodeBad = 0, nodeCnt[RN_COUNT] = {0};
    for (auto& n : w.nodes) {
        nodeCnt[n.kind]++;
        nodeTotals[n.kind]++;
        if (!rc.nearReach(n.x, n.y, 2)) {
            nodeBad++;
            printf("    unreachable %s at tile (%d,%d)\n", NODE_NAME[n.kind], (int)(n.x / 16.f), (int)(n.y / 16.f));
        }
    }
    printf("  resource nodes:");
    for (int k = 0; k < RN_COUNT; k++)
        if (nodeCnt[k]) printf(" %s=%d", NODE_NAME[k], nodeCnt[k]);
    printf("\n  nodes unreachable: %d of %zu\n", nodeBad, w.nodes.size());
    if (nodeBad) bad++;
    // the edge wall must be solid everywhere except inside the gates' zones
    int hole = 0, gateOpen = 0, gateBlocked = 0;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
            if ((x < 2 || y < 2 || x >= MW - 2 || y >= MH - 2) && !w.solidTile(x, y)) {
                if (portalAt(map, x, y)) gateOpen++;
                else hole++;
            }
    for (int i = 0; i < MD.nPortals; i++) {
        const Portal& pt = MD.portals[i];
        const V2 c = portalPoint(w, pt);
        const bool ok = rc.nearReach(c.x, c.y, 2);
        printf("  gate to %-9s reachable: %s\n", MAPS[pt.toMap].name, ok ? "yes" : "NO");
        if (!ok) gateBlocked++;
        // the arrival point on the other side must be a valid free spot inside that map's walls
        const V2 a = g.freeSpot(pt.arriveTx, pt.arriveTy);
        if (a.x < 3 * TS || a.y < 3 * TS || a.x > (MW - 3) * TS || a.y > (MH - 3) * TS) {
            printf("  PROBLEM: arrival point of gate to %s is outside the playable area\n", MAPS[pt.toMap].name);
            bad++;
        }
    }
    printf("  edge wall holes outside gates: %d (gate tiles open: %d)\n", hole, gateOpen);
    if (hole || gateBlocked) bad++;
    printf("  entry reachable: %s\n", rc.at(arrive.x, arrive.y) >= 0 ? "yes" : "NO");
    // walk test: the real player walks from the entry to the hub
    {
        const int f = driveTo(g, w.hub[map], 0);
        printf("  walk from the entry to the hub: %s after %.0fs (landmarks discovered %d)\n", f >= 0 ? "arrived" : "FAILED", f >= 0 ? f / 60.f : 240.f, g.landmarksFound());
        if (f < 0) bad++;
    }
    return bad;
}

static int worldCheck(Game& g) {
    int bad = 0;
    long nodeTotals[RN_COUNT] = {0};
    printf("maps: %d, each %dx%d tiles (the original Wildwood area is %dx%d at tile (%d,%d) of the home map)\n", B_COUNT, MW, MH, CW, CH, COX, COY);
    for (int m = 0; m < B_COUNT; m++) bad += mapCheck(g, m, nodeTotals);
    {
        printf("all resource nodes over the five maps:");
        for (int k = 0; k < RN_COUNT; k++) printf(" %s=%ld", NODE_NAME[k], nodeTotals[k]);
        printf("\n");
        for (int k = 0; k < RN_COUNT; k++)
            if (!nodeTotals[k]) {
                printf("  PROBLEM: no %s anywhere\n", NODE_NAME[k]);
                bad++;
            }
    }
    // travel test: walk through every gate with the real player, check the arrival, then walk back through the gate home
    for (int m = B_DESERT; m < B_COUNT; m++) {
        enterMap(g, B_HOME);
        g.p.pos = g.world.spawn;
        g.p.coins = 777;
        g.p.inv.add(itemMat(M_WOOD), 5);
        g.p.hp = 77;
        g.p.sk.s[SK_MINING].xp = 123;
        int gi = -1;
        for (int i = 0; i < MAPS[B_HOME].nPortals; i++)
            if (MAPS[B_HOME].portals[i].toMap == m) gi = i;
        const Portal& pt = MAPS[B_HOME].portals[gi];
        const V2 c = portalPoint(g.world, pt);
        const int f = driveTo(g, c, 1);
        const bool there = f >= 0 && g.world.mapId == m && g.curBiome == m;
        const V2 pp = g.p.pos;
        const bool valid = !g.world.blocked(pp.x, pp.y, 5.f) && pp.x > 2 * TS && pp.y > 2 * TS && pp.x < (MW - 2) * TS && pp.y < (MH - 2) * TS;
        const bool kept = g.p.coins == 777 && g.p.mat(M_WOOD) == 5 && g.p.sk.s[SK_MINING].xp == 123 && (int)g.p.hp <= 100;
        printf("  walk through the gate home -> %-9s: %s after %.1fs, map %d, valid spot %s, hunter's state kept %s\n", MAPS[m].name, there ? "arrived" : "FAILED", f / 60.f, g.world.mapId, valid ? "yes" : "NO", kept ? "yes" : "NO");
        if (!there || !valid || !kept) bad++;
        // and back
        g.travelCd = 0;
        const Portal& back = MAPS[m].portals[0];
        const V2 bc = portalPoint(g.world, back);
        const int f2 = driveTo(g, bc, 1);
        const bool home = f2 >= 0 && g.world.mapId == B_HOME;
        const V2 q = g.p.pos;
        const bool valid2 = !g.world.blocked(q.x, q.y, 5.f);
        printf("  walk through the gate %-9s -> home: %s after %.1fs, valid spot %s\n", MAPS[m].name, home ? "arrived" : "FAILED", f2 / 60.f, valid2 ? "yes" : "NO");
        if (!home || !valid2) bad++;
    }

    // terrain + climate mechanics, measured through the real player update (each in the map where that ground exists)
    {
        auto findPatch = [&](const World& w, uint8_t t, V2& out, int rad = 4) {
            for (int y = 8; y < MH - 8; y++)
                for (int x = 8; x < MW - 8; x++) {
                    bool ok = true;
                    for (int j = -rad; j <= rad && ok; j++)
                        for (int i = -rad; i <= rad && ok; i++)
                            if (w.tile[y + j][x + i] != t || w.solid[y + j][x + i]) ok = false;
                    if (ok) {
                        out = V2(x * TS + 8.f, y * TS + 8.f);
                        return true;
                    }
                }
            return false;
        };
        struct Row {
            const char* name;
            uint8_t tile;
            int biome;
        } rows[] = {{"grass", T_GRASS, B_HOME}, {"sand", T_SAND, B_DESERT}, {"dune", T_DUNE, B_DESERT}, {"mud", T_MUD, B_SWAMP},
                    {"murk", T_MURK, B_SWAMP}, {"snow", T_SNOW, B_SNOW}, {"packed snow", T_SNOWP, B_SNOW}, {"ice", T_ICE, B_SNOW}};
        float grassD = 0;
        for (auto& rw : rows) {
            enterMap(g, rw.biome);
            V2 at;
            if (!findPatch(g.world, rw.tile, at, rw.tile == T_MUD ? 2 : 4)) {
                printf("  terrain %-12s no 9x9 patch found (skipped)\n", rw.name);
                continue;
            }
            g.p.pos = at;
            g.in.right = true;
            for (int f = 0; f < 30; f++) {
                g.p.hp = g.p.maxhp;
                g.update(1.f / 60.f);
            }
            float moved = g.p.pos.x - at.x;
            g.in.right = false;
            V2 rel = g.p.pos;
            for (int f = 0; f < 30; f++) g.update(1.f / 60.f);
            float coast = g.p.pos.x - rel.x;
            if (rw.tile == T_GRASS) grassD = moved;
            printf("  terrain %-12s walked %.1f px in 0.5s (%.0f%% of grass), coasted %.1f px after release\n", rw.name, moved, grassD > 0 ? 100.f * moved / grassD : 100.f, coast);
        }
        // stamina: sprint for 1s in each climate (midday), starting from full
        struct Cl {
            int biome;
            const char* name;
            uint8_t tile;
        } cls[] = {{B_HOME, "home", T_GRASS}, {B_DESERT, "desert", T_SAND}, {B_SWAMP, "swamp", T_MUD}, {B_SNOW, "snow", T_SNOW}, {B_MOUNTAIN, "mountain", T_STONE}};
        for (int armor = 0; armor <= 2; armor += 2)
            for (auto& c : cls) {
                if (armor == 2 && c.biome != B_SNOW) continue;
                enterMap(g, c.biome);
                V2 at;
                if (!findPatch(g.world, c.tile, at, c.tile == T_MUD ? 2 : 4)) continue;
                g.dayT = 60;  // midday
                g.p.up[U_ARMOR] = armor;
                g.p.pos = at;
                g.in.right = true;
                g.in.sprint = true;
                float s0 = g.p.stamina;
                for (int f = 0; f < 60; f++) {
                    g.p.hp = g.p.maxhp;
                    g.dayT = 60;
                    g.update(1.f / 60.f);
                }
                g.in.right = g.in.sprint = false;
                printf("  sprint 1s in %-8s%s: stamina %.1f -> %.1f (drain %.1f)\n", c.name, armor == 2 ? " + fur coat" : "", s0, g.p.stamina, s0 - g.p.stamina);
            }
        g.in.right = g.in.sprint = false;
    }
    printf(bad ? "WORLDCHECK: %d PROBLEM(S)\n" : "WORLDCHECK: ok (%d problems)\n", bad);
    return bad;
}

static void tour(Game& g, const std::string& dir) {
    auto frames = [&](int n) {
        for (int i = 0; i < n; i++) {
            g.update(1.f / 60.f);
            g.in.lPressed = g.in.lReleased = g.in.ePressed = g.in.escPressed = g.in.enterPressed = false;
        }
        g.render();
    };
    auto shot = [&](const std::string& name) { g.saveCanvas((dir + "/" + name + ".bmp").c_str()); };
    static const char* nm[B_COUNT] = {"home", "desert", "swamp", "snow", "mountain"};
    for (int b = 0; b < B_COUNT; b++) {
        enterMap(g, b);
        g.showMap = true;
        g.respawnT = 1e9f;
        // the entrance, the hub, then every landmark of the map
        std::vector<std::pair<std::string, V2>> spots;
        spots.push_back({std::string(nm[b]) + "_entry", g.p.pos});
        spots.push_back({std::string(nm[b]) + "_hub", b == B_HOME ? g.world.spawn : g.world.hub[b]});
        int i = 0;
        for (auto& l : g.world.landmarks) spots.push_back({std::string(nm[b]) + "_lm" + std::to_string(i++), V2(l.x, l.y + 24)});
        int k = 0;
        for (auto& sp : spots) {
            g.p.pos = sp.second;
            g.dayT = 60;  // midday
            g.travelCd = 1e9f;  // a tour must not walk through a gate by accident
            g.setCam(0, true);
            frames(25);
            shot(sp.first);
            if (k == 1) {  // also a night view of each hub, for lanterns / fog / glows
                g.dayT = 300.f * 0.6f;
                frames(20);
                shot(sp.first + "_night");
            }
            k++;
        }
        // frame cost (software renderer, so only the relative numbers mean anything)
        g.p.pos = b == B_HOME ? g.world.spawn : g.world.hub[b];
        g.setCam(0, true);
        Uint64 t0 = SDL_GetPerformanceCounter();
        for (int i2 = 0; i2 < 120; i2++) {
            g.update(1.f / 60.f);
            g.render();
        }
        double ms = (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency() / 120.0;
        printf("  frame cost at %-8s hub: %.2f ms (update+render)\n", nm[b], ms);
        // the gates of this map, with the label and minimap marker
        for (int gi = 0; gi < MAPS[b].nPortals; gi++) {
            const V2 c = portalPoint(g.world, MAPS[b].portals[gi]);
            const int side = MAPS[b].portals[gi].side;
            g.p.pos = c + (side == 0 ? V2(-70, 0) : side == 3 ? V2(70, 0) : side == 1 ? V2(0, -50) : V2(0, 50));
            g.dayT = 60;
            g.setCam(0, true);
            frames(25);
            shot(std::string(nm[b]) + "_gate_to_" + nm[MAPS[b].portals[gi].toMap]);
        }
    }
}


// ---- wildlife verification (flag: --wildlife DIR): species gallery, spawn simulation, drops, legendary limits
static void wildlife(Game& g, const std::string& dir) {
    g.newGame();
    g.state = ST_PLAY;
    g.helpT = 0;
    auto shot = [&](const std::string& name) { g.saveCanvas((dir + "/" + name + ".bmp").c_str()); };
    // 1) gallery: every species, all four tiers, per biome group (rows = tiers, columns = species)
    struct Grp { const char* n; std::vector<int> k; } grp[5] = {
        {"home", {A_RABBIT, A_DEER, A_BOAR, A_WOLF}},
        {"desert", {A_HARE, A_FENNEC, A_COYOTE, A_RATTLER, A_ROADRUNNER, A_LIZARD, A_DUNELION}},
        {"swamp", {A_FROG, A_DUCK, A_MARSHDEER, A_MOCCASIN, A_GATOR, A_PANTHER}},
        {"snow", {A_ARCTICFOX, A_SNOWHARE, A_REINDEER, A_MOOSE, A_LYNX, A_POLARBEAR}},
        {"mountain", {A_GOAT, A_BIGHORN, A_EAGLE, A_MARMOT, A_MTLION}}};
    g.dayT = 60;
    for (auto& gr : grp)
        for (int form = 0; form < 2; form++) {
            g.render();
            g.fillRect(0, 0, LW, LH, HEX(0x6f7d5a));
            g.camX = g.camY = 0;
            int n = (int)gr.k.size();
            for (int t = 0; t < 4; t++) {
                g.text(2, 14 + t * 52, TIER[t].name, TIER[t].col, 1, true);
                for (int i = 0; i < n; i++) {
                    int k = gr.k[i];
                    int f = ANIMAL[k].forms > 1 ? form : 0;
                    float px = 58 + i * 48.f, py = 48 + t * 52.f;
                    g.sprW(g.A.shadow[1], px, py, false, 0.6f);
                    g.sprW(g.A.animal[k][f + 2 * t][(int)(g.timeAcc) & 3 ? 1 : 0], px, py);
                    if (t == 0) g.textC((int)px, 4, std::string(animalName(k, 0)).substr(0, 11), C_TEXT, 1, true);
                }
            }
            shot(std::string("gallery_") + gr.n + (form ? "_form1" : "_form0"));
        }
    // 2) tier odds over many rolls (no cooldown, nothing alive)
    {
        g.animals.clear();
        int cnt[4] = {0};
        const int N = 400000;
        for (int i = 0; i < N; i++) {
            g.legCool = 0;
            g.animals.clear();
            cnt[g.rollTier()]++;
        }
        printf("tier odds over %d rolls: common %.2f%%  uncommon %.2f%%  rare %.2f%%  legendary %.3f%%\n", N, 100.0 * cnt[0] / N, 100.0 * cnt[1] / N, 100.0 * cnt[2] / N, 100.0 * cnt[3] / N);
        // with a live legendary, or on cooldown, no legendary may appear
        g.animals.clear();
        g.spawnAnimal(A_DEER, g.p.pos, 0, TR_LEGENDARY);
        int leg = 0;
        for (int i = 0; i < N; i++) {
            g.legCool = 0;
            leg += g.rollTier() == TR_LEGENDARY;
        }
        printf("legendaries rolled while one is alive: %d (must be 0)\n", leg);
        g.animals.clear();
        g.legCool = 300;
        leg = 0;
        for (int i = 0; i < N; i++) {
            g.legCool = 300;
            leg += g.rollTier() == TR_LEGENDARY;
        }
        printf("legendaries rolled during cooldown: %d (must be 0)\n", leg);
    }
    // 3) kill every species at tier 0 and 3: drops must be non-empty, tiers must pay more
    {
        printf("drops (tier0 -> legendary), coins gained at legendary:\n");
        for (int k = 0; k < A_COUNT; k++) {
            int tot[2];
            int coins = 0;
            for (int ti = 0; ti < 2; ti++) {
                g.animals.clear();
                int c0 = g.p.coins;
                Animal* a = g.spawnAnimal(k, g.p.pos + V2(0, 40), ANIMAL[k].forms > 1 ? 1 : 0, ti ? 3 : 0);
                g.killAnimal(*a);
                tot[ti] = g.animals[0].totalRemain;
                if (ti) coins = g.p.coins - c0;
            }
            printf("  %-14s %2d -> %2d  +%dC%s\n", animalName(k, 0).c_str(), tot[0], tot[1], coins, tot[0] <= 0 || tot[1] < tot[0] ? "   <-- PROBLEM" : "");
        }
    }
    // 5) behaviour probes
    {
        enterMap(g, B_DESERT);
        g.p.pos = g.world.hub[B_DESERT];
        g.p.maxhp = g.p.hp = 1e6f;
        const char* stn[5] = {"wander", "flee", "chase", "feed", "rest"};
        for (int k : {(int)A_COYOTE, (int)A_MTLION, (int)A_BIGHORN}) {
            for (int night = 0; night < 2; night++) {
                g.animals.clear();
                g.dayT = night ? 300.f * 0.6f : 60.f;
                Animal* a = g.spawnAnimal(k, g.p.pos + V2(60, 0), 0, 0);
                for (int f = 0; f < 90; f++) g.update(1.f / 60.f), g.p.hp = g.p.maxhp, g.dayT = night ? 300.f * 0.6f : 60.f;
                printf("probe %-14s %-5s player 60px away for 1.5s -> %s\n", animalName(k, 0).c_str(), night ? "night" : "day", stn[g.animals[0].st]);
                (void)a;
            }
        }
        g.dayT = 60;
        int seen[5] = {0};
        enterMap(g, B_MOUNTAIN);
        g.respawnT = 4.f;
        g.p.pos = g.world.hub[B_MOUNTAIN];
        for (int i = 0; i < 6; i++) g.spawnAnimal(A_MARMOT, g.p.pos + V2(300 + i * 5, 40 * i), 0, 0), g.spawnAnimal(A_GOAT, g.p.pos + V2(330, 40 * i - 100), 0, 0);
        for (int f = 0; f < 60 * 120; f++) {
            g.update(1.f / 60.f);
            if (f % 30 == 0) for (auto& a : g.animals) if (!a.dead) seen[a.st]++;
        }
        printf("marmot+goat state samples over 2 min: wander %d  feed %d  rest %d\n", seen[0], seen[3], seen[4]);
    }
    // 4) spawn simulation: 10 minutes standing in each new biome, day and night
    for (int b = 1; b < B_COUNT; b++) {
        enterMap(g, b);
        g.respawnT = 4.f;
        memset(g.spawnStat, 0, sizeof g.spawnStat);
        g.p.pos = g.world.hub[b];
        g.p.maxhp = g.p.hp = 1e6f;
        g.legCool = 0;
        int maxLive = 0, maxLeg = 0;
        double sumLive = 0;
        int samples = 0;
        Uint64 t0 = SDL_GetPerformanceCounter();
        const int FR = 60 * 600;
        for (int f = 0; f < FR; f++) {
            g.p.hp = g.p.maxhp;
            g.update(1.f / 60.f);
            if (f % 60 == 0) {
                int live = 0, leg = 0;
                for (auto& a : g.animals)
                    if (!a.dead) live++, leg += a.tier == TR_LEGENDARY;
                maxLive = std::max(maxLive, live);
                maxLeg = std::max(maxLeg, leg);
                sumLive += live;
                samples++;
            }
        }
        double ms = (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency() / FR;
        printf("%s: live animals avg %.1f max %d (cap %d), max legendaries alive %d, sim cost %.3f ms/frame\n", BIOME[b].name, sumLive / samples, maxLive, MAX_LIVE_ANIMALS, maxLeg, ms);
        for (int k = 0; k < A_COUNT; k++) {
            int s = g.spawnStat[k][0] + g.spawnStat[k][1] + g.spawnStat[k][2] + g.spawnStat[k][3];
            if (s)
                printf("    %-12s spawned %3d  (unc %d, rare %d, leg %d)\n", animalName(k, 0).c_str(), s, g.spawnStat[k][1], g.spawnStat[k][2], g.spawnStat[k][3]);
        }
    }
}

// ---- weapon / item verification (flag: --weapons): registry consistency, bow and crossbow behaviour, drops, save round trip
static int weaponTest(Game& g) {
    int bad = 0;
    // the save round trip below writes the save file: keep the player's real one safe and put it back afterwards
    std::string savedBytes;
    bool hadSave = false;
    {
        std::ifstream in(g.savePath(), std::ios::binary);
        if (in) {
            hadSave = true;
            savedBytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
    }
    auto check = [&](bool ok, const char* what) {
        printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
        if (!ok) bad++;
    };
    // --- registry
    {
        std::set<std::string> names;
        bool ids = true, stacks = true, prices = true, refs = true;
        for (int i = 0; i < ITEM_COUNT; i++) {
            const ItemDef& d = item(i);
            names.insert(d.name);
            if (d.name.empty()) ids = false;
            if (d.stack <= 0) stacks = false;
            if (d.kind == IK_MATERIAL && d.price != MAT_PRICE[d.ref]) prices = false;
            if (d.kind == IK_MATERIAL && (itemMat(d.ref) != i || d.stack != 99)) refs = false;
            if (d.kind == IK_LAUNCHER && d.stack != 1) refs = false;
            if (d.kind == IK_THROWN && itemWpn(d.ref) != i) refs = false;
            if (d.kind == IK_AMMO && itemAmmo(d.ref) != i) refs = false;
        }
        printf("item registry: %d items\n", ITEM_COUNT);
        check(ids && (int)names.size() == ITEM_COUNT, "every item has a unique, non-empty name");
        check(stacks, "every stack size is positive");
        check(prices, "material prices in the registry match the price table");
        check(refs, "ids map back to their legacy tables; materials stack to 99; launchers do not stack");
        check(item(ITEM_PACK0).slots == 20 && item(ITEM_PACK0 + 1).slots == 30 && item(ITEM_PACK0 + 2).slots == 40, "backpacks provide 20 / 30 / 40 slots");
        check(!isLauncher(W_FLINT) && !isLauncher(W_JAVELIN) && isLauncher(W_BOW) && isLauncher(W_CROSSBOW), "only the bow and crossbow are launchers");
    }
    g.newGame();
    g.state = ST_PLAY;
    g.helpT = 0;
    g.animals.clear();
    g.items.clear();
    g.p.hp = g.p.maxhp = 1e6f;
    auto step = [&](int n) {
        for (int i = 0; i < n; i++) {
            g.update(1.f / 60.f);
            g.in.lPressed = g.in.lReleased = g.in.ePressed = false;
        }
    };
    // spawn a sturdy, unaware deer 150 px to the right and aim at it
    // find an open lane (no props or water for 260 px to the right) so obstacles don't get in the way of the test
    V2 lane = g.world.spawn;
    for (int ty = COY + 10; ty < COY + CH - 10; ty++) {
        bool found = false;
        for (int tx = COX + 12; tx < COX + CW - 30 && !found; tx++) {
            bool clear = true;
            for (float x = 0; x <= 260 && clear; x += 8)
                for (float dy : {-12.f, 0.f, 9.f, 14.f}) clear = clear && !g.world.blocked(tx * TS + x, ty * TS + dy, 7);
            if (clear) {
                lane = V2(tx * TS + 8.f, ty * TS + 8.f);
                found = true;
            }
        }
        if (found) break;
    }
    auto target = [&]() -> Animal* {
        g.animals.clear();
        g.p.pos = lane;
        g.setCam(0, true);
        Animal* a = g.spawnAnimal(A_DEER, g.p.pos + V2(150, 0), 0, 0);
        a->maxhp = a->hp = 100000;
        a->st = AS_REST;
        a->stT = 1e9f;
        a->idleT = 0;
        g.in.mx = a->pos.x - g.camX;
        g.in.my = a->pos.y - a->bodyH - g.camY;
        return a;
    };
    // hold the button for `secs`, then release. Returns damage dealt to the target.
    auto shoot = [&](float secs) -> int {
        Animal* a = target();
        int hp0 = a->hp;
        g.in.l = true;
        g.in.lPressed = true;
        step(1);
        int n = (int)(secs * 60);
        if (n > 1) step(n - 1);
        g.in.l = false;
        g.in.lReleased = true;
        step(1);
        step(40);  // flight
        return hp0 - g.animals[0].hp;
    };
    g.p.cd = 0;
    // --- bow (set WEAPON_SHOTS=dir to also write two screenshots: bow mid-draw and an arrow in flight)
    if (const char* sd = getenv("WEAPON_SHOTS")) {
        g.p.inv.set(itemWpn(W_BOW), 1);
        g.p.sel = W_BOW;
        g.p.inv.set(itemAmmo(AM_ARROW), 12);
        g.p.inv.set(itemWpn(W_CROSSBOW), 1);
        g.p.inv.set(itemAmmo(AM_BOLT), 6);
        Animal* a = target();
        (void)a;
        g.p.cd = 0;
        g.in.l = true; g.in.lPressed = true; step(1); step(35);
        g.render();
        g.saveCanvas((std::string(sd) + "/bow_draw.bmp").c_str());
        g.in.l = false; g.in.lReleased = true; step(1); step(4);
        g.render();
        g.saveCanvas((std::string(sd) + "/bow_flight.bmp").c_str());
        step(60);
        g.p.sel = W_CROSSBOW;
        g.p.cd = 0;
        g.in.l = true; g.in.lPressed = true; step(1); g.in.l = false; g.in.lReleased = true; step(3);
        g.render();
        g.saveCanvas((std::string(sd) + "/crossbow_flight.bmp").c_str());
        g.items.clear();
    }
    g.p.inv.set(itemWpn(W_BOW), 1);
    g.p.sel = W_BOW;
    g.p.inv.set(itemAmmo(AM_ARROW), 200);
    long full = 0, tap = 0;
    int fullHits = 0, tapHits = 0;
    const int N = 40;
    for (int i = 0; i < N; i++) {
        g.p.inv.set(itemAmmo(AM_ARROW), 50);
        g.p.cd = 0;
        int d = shoot(1.0f);
        full += d;
        fullHits += d > 0;
        g.p.cd = 0;
        g.p.inv.set(itemAmmo(AM_ARROW), 50);
        d = shoot(0.1f);
        tap += d;
        tapHits += d > 0;
    }
    printf("bow: full draw avg %.1f dmg (%d/%d hit), quick tap avg %.1f dmg (%d/%d hit)\n", (double)full / N, fullHits, N, (double)tap / N, tapHits, N);
    check(fullHits == N, "a full draw reaches and hits a target 150px away every time");
    check((double)full / N > (double)tap / N * 1.4, "a full draw hits clearly harder than a quick tap");
    // ammo use
    g.p.inv.set(itemAmmo(AM_ARROW), 5);
    g.p.cd = 0;
    shoot(1.0f);
    check(g.p.ammo(AM_ARROW) == 4, "one shot costs exactly one arrow");
    check(g.p.wpn(W_BOW) == 1, "the bow itself is not used up");
    // recovery: arrows that hit either break or drop; walking over a dropped one gives it back
    g.p.inv.set(itemAmmo(AM_ARROW), 20);
    int fired = 0, recovered = 0;
    for (int i = 0; i < 30; i++) {
        g.items.clear();
        g.p.cd = 0;
        int before = g.p.ammo(AM_ARROW);
        shoot(1.0f);
        fired++;
        if (!g.items.empty()) {
            g.p.pos = g.items[0].pos;
            step(2);
            if (g.p.ammo(AM_ARROW) == before) recovered++;
        }
    }
    printf("arrows: %d fired at a target, %d recovered by walking over them (expected about 65%%)\n", fired, recovered);
    check(recovered > 8 && recovered < fired, "some arrows are recovered and some break");
    // no ammo
    g.p.inv.set(itemAmmo(AM_ARROW), 0);
    g.p.cd = 0;
    int dmg0 = shoot(1.0f);
    check(dmg0 == 0 && g.p.ammo(AM_ARROW) == 0 && g.projs.empty(), "no arrows: the bow does not fire");
    // --- crossbow
    g.p.inv.set(itemWpn(W_CROSSBOW), 1);
    g.p.sel = W_CROSSBOW;
    g.p.inv.set(itemAmmo(AM_BOLT), 10);
    g.p.cd = 0;
    int cd1 = shoot(0.05f);
    check(cd1 > 0 && g.p.ammo(AM_BOLT) == 9, "the crossbow fires at once on click and uses one bolt");
    {
        Animal* a = target();
        (void)a;
        g.p.cd = 0;
        g.in.l = true; g.in.lPressed = true; step(1); g.in.l = false; g.in.lReleased = true; step(1);
        int afterFirst = g.p.ammo(AM_BOLT);
        step(30);  // 0.5 s later: still reloading
        g.in.l = true; g.in.lPressed = true; step(1); g.in.l = false; g.in.lReleased = true; step(1);
        check(g.p.ammo(AM_BOLT) == afterFirst, "no second shot while the crossbow reloads (0.5 s)");
        step(75);  // past the 1.6 s reload
        g.in.l = true; g.in.lPressed = true; step(1); g.in.l = false; g.in.lReleased = true; step(1);
        check(g.p.ammo(AM_BOLT) == afterFirst - 1, "it fires again once reloaded (about 1.6 s)");
    }
    long cb = 0;
    for (int i = 0; i < 20; i++) {
        g.p.inv.set(itemAmmo(AM_BOLT), 10);
        g.p.cd = 0;
        cb += shoot(0.05f);
    }
    printf("crossbow avg %.1f dmg vs bow full draw %.1f, javelin base %d\n", (double)cb / 20, (double)full / N, (int)WPN[W_JAVELIN].dmg);
    check((double)cb / 20 > (double)full / N, "a crossbow bolt hits harder than a full-draw arrow");
    // --- thrown weapons are unchanged
    g.p.sel = W_JAVELIN;
    g.p.inv.set(itemWpn(W_JAVELIN), 3);
    g.p.cd = 0;
    int jd = shoot(0.5f);
    check(jd > 0 && g.p.wpn(W_JAVELIN) == 2, "a javelin is still thrown, hits, and is used up (to be picked up again)");
    // --- safety net: only a bow with no arrows counts as having no weapon
    for (int w = 0; w < W_COUNT; w++) g.p.inv.set(itemWpn(w), 0);
    g.p.inv.set(itemWpn(W_BOW), 1);
    g.p.inv.set(itemAmmo(AM_ARROW), 0);
    g.p.inv.set(itemAmmo(AM_BOLT), 0);
    g.p.coins = 0;
    for (int m = 0; m < M_COUNT; m++) g.p.inv.set(itemMat(m), 0);
    g.projs.clear();
    g.items.clear();
    g.stuckCheckT = 0;
    step(70);
    check(g.p.wpn(W_FLINT) == FLINT_LOAN, "stranded with a bow, no arrows and no money: the merchant lends flint knives");
    // --- new materials drop from the new species
    {
        struct E { int kind, mat; } ex[] = {{A_DUCK, M_FEATHER}, {A_EAGLE, M_FEATHER}, {A_RATTLER, M_SCALE}, {A_GATOR, M_SCALE},
                                            {A_GOAT, M_HORN}, {A_BIGHORN, M_HORN}, {A_MTLION, M_CLAW}, {A_POLARBEAR, M_CLAW}};
        bool all = true;
        for (auto& e : ex) {
            int got = 0;
            for (int i = 0; i < 12; i++) {
                g.animals.clear();
                Animal* a = g.spawnAnimal(e.kind, g.p.pos + V2(40, 0), 0, 0);
                g.killAnimal(*a);
                got += g.animals[0].remain[e.mat];
            }
            if (got <= 0) {
                all = false;
                printf("    %s never dropped %s in 12 kills\n", animalName(e.kind, 0).c_str(), MAT_NAME[e.mat]);
            }
        }
        check(all, "feathers, scales, horns and claws drop from the species they were assigned to");
        int orig = 0;
        for (int k : {A_RABBIT, A_DEER, A_BOAR, A_WOLF})
            for (int m = M_FEATHER; m < M_CARCASS_N; m++) orig += ANIMAL[k].drop[m][1];
        check(orig == 0, "the original four animals drop no new materials");
    }
    // --- save round trip
    {
        g.p.coins = 123;
        g.p.up[U_BAG] = 2;  // the test stores 11 materials + 9 weapons + 2 ammo types: more than the starting 20 slots
        g.p.syncPack();
        for (int m = 0; m < M_CARCASS_N; m++) g.p.inv.set(itemMat(m), m + 1);
        for (int w = 0; w < W_COUNT; w++) g.p.inv.set(itemWpn(w), isLauncher(w) ? 1 : w + 2);
        g.p.inv.set(itemAmmo(AM_ARROW), 37);
        g.p.inv.set(itemAmmo(AM_BOLT), 11);
        g.save();
        Player keep = g.p;
        g.p.coins = 0;
        for (int m = 0; m < M_COUNT; m++) g.p.inv.set(itemMat(m), 0);
        for (int w = 0; w < W_COUNT; w++) g.p.inv.set(itemWpn(w), 0);
        g.p.inv.clear();
        bool ld = g.load();
        bool same = ld && g.p.coins == keep.coins && g.p.ammo(AM_ARROW) == 37 && g.p.ammo(AM_BOLT) == 11;
        for (int m = 0; m < M_CARCASS_N; m++) same = same && g.p.mat(m) == keep.mat(m);
        for (int w = 0; w < W_COUNT; w++) same = same && g.p.wpn(w) == keep.wpn(w);
        check(same, "save / load round trip keeps coins, all 11 animal materials, all 9 weapons and both ammo types");
    }
    if (hadSave) {
        std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
        out << savedBytes;
    } else
        std::remove(g.savePath().c_str());
    printf("WEAPONS: %s (%d failed)\n", bad ? "FAILED" : "ok", bad);
    return bad;
}

// ---- inventory verification (flag: --selftest): the Inventory class, the game rules built on it, and save v2 / migration
static int selfTest(Game& g) {
    int bad = 0;
    auto check = [&](bool ok, const char* what) {
        printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
        if (!ok) bad++;
    };
    const int MEAT = itemMat(M_MEAT), HIDE = itemMat(M_HIDE), BONE = itemMat(M_BONE);
    printf("inventory class\n");
    {
        Inventory inv(10);
        check(inv.add(MEAT, 250) == 250 && inv.usedSlots() == 3 && inv.count(MEAT) == 250, "250 meat = stacks of 99, 99, 52 (3 slots)");
        check(inv.slots[0].n == 99 && inv.slots[1].n == 99 && inv.slots[2].n == 52, "stacks fill up to exactly 99");
        check(inv.remove(MEAT, 60) == 60 && inv.count(MEAT) == 190 && inv.usedSlots() == 2, "removing 60 empties the 52-stack first, then 8 from another");
        check(inv.remove(MEAT, 9999) == 190 && inv.usedSlots() == 0, "removing more than owned removes only what exists");
        Inventory tiny(2);
        check(tiny.add(MEAT, 300) == 198 && tiny.room(MEAT) == 0 && !tiny.canAdd(HIDE, 1), "a full bag takes a partial stack and then refuses");
        check(tiny.add(MEAT, 1) == 0, "adding to a full bag adds nothing");
        Inventory bag(5);
        check(bag.add(itemWpn(W_BOW), 3) == 3 && bag.usedSlots() == 3, "launchers never stack: 3 bows use 3 slots");
        check(bag.add(itemWpn(W_FLINT), 31) == 31 && bag.usedSlots() == 5, "thrown weapons stack to 30: 31 knives use 2 slots");
        check(bag.add(MEAT, 1) == 0, "no slot left for anything new");
        Inventory c(3);
        c.add(MEAT, 40);
        c.add(HIDE, 5);
        c.add(MEAT, 70);  // tops up the first stack to 99, remainder into a new slot -> all 3 slots in use
        check(c.usedSlots() == 3 && c.count(MEAT) == 110, "an existing partial stack is topped up before a new slot is used");
        c.remove(MEAT, 100);
        c.compact();
        check(c.usedSlots() == 2 && c.count(MEAT) == 10 && c.count(HIDE) == 5, "compact merges partial stacks");
        // growth / shrink
        Inventory gr(20);
        for (int m = 0; m < M_CARCASS_N; m++) gr.add(itemMat(m), 5);
        check(gr.setCapacity(30) && gr.capacity() == 30 && gr.usedSlots() == M_CARCASS_N, "growing the bag keeps every item");
        check(!gr.setCapacity(5) && gr.capacity() == 30 && gr.usedSlots() == M_CARCASS_N, "shrinking below what is stored is refused (nothing is lost)");
        check(gr.setCapacity(M_CARCASS_N) && gr.capacity() == M_CARCASS_N, "shrinking to exactly what fits works");
        // random operations against a reference model
        Inventory z(20);
        std::map<int, long> ref;
        Rng r(12345);
        bool okInv = true;
        for (int i = 0; i < 40000 && okInv; i++) {
            int it = r.irange(0, ITEM_COUNT - 1);
            if (r.chance(0.55f)) {
                int n = r.irange(1, 120), got = z.add(it, n);
                ref[it] += got;
                if (got < n && z.room(it) != 0) okInv = false;  // a short add must mean the item really has no room left
            } else {
                int n = r.irange(1, 130), got = z.remove(it, n);
                if (got != std::min<long>(n, ref[it])) okInv = false;
                ref[it] -= got;
            }
            long total = 0;
            for (auto& sl : z.slots) {
                if (sl.item >= 0 && (sl.n <= 0 || sl.n > item(sl.item).stack)) okInv = false;
                if (sl.item < 0 && sl.n != 0) okInv = false;
                total += sl.n;
            }
            if (z.count(it) != ref[it]) okInv = false;
            if (z.usedSlots() > z.capacity()) okInv = false;
        }
        check(okInv, "40000 random adds/removes: counts match a reference model, no stack over its limit, no phantom slots");
        // keys
        bool keys = true;
        std::set<std::string> ks;
        for (int i = 0; i < ITEM_COUNT; i++) {
            keys = keys && itemFromKey(itemKey(i)) == i;
            ks.insert(itemKey(i));
        }
        check(keys && (int)ks.size() == ITEM_COUNT && itemFromKey("no_such_item") == -1, "every item has a unique stable save key that maps back to it");
        check(item(MEAT).stack == 99 && item(itemMat(M_CLAW)).stack == 99, "materials stack to 99");
        check(PACK_SLOTS[0] == 20 && PACK_SLOTS[1] == 30 && PACK_SLOTS[2] == 40 && PACK_SLOTS[3] == 50, "backpack levels give 20 / 30 / 40 / 50 slots");
    }
    // ---- game rules
    std::string savedBytes;
    bool hadSave = false;
    {
        std::ifstream in(g.savePath(), std::ios::binary);
        if (in) {
            hadSave = true;
            savedBytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
    }
    printf("game rules\n");
    g.newGame();
    g.state = ST_PLAY;
    g.helpT = 0;
    check(g.p.bagCap() == 20 && g.p.bagUsed() == 2 && g.p.wpn(W_FLINT) == 8 && g.p.bandages() == 1, "a new hunter has a 20-slot pack with 8 flint knives and 1 bandage (2 slots)");
    // fill every slot with something other than meat/bone/hide
    auto fillWithJunk = [&]() {
        g.p.inv.clear();
        int slotsLeft = g.p.bagCap();
        for (int m = M_TUSK; m < M_CARCASS_N && slotsLeft > 0; m++, slotsLeft--) g.p.inv.add(itemMat(m), 50);
        for (int w = 0; w < W_COUNT && slotsLeft > 0; w++, slotsLeft--) g.p.inv.add(itemWpn(w), 1);
        for (int a = 0; a < AM_COUNT && slotsLeft > 0; a++, slotsLeft--) g.p.inv.add(itemAmmo(a), 10);
        g.p.inv.add(ITEM_CONS0 + C_BANDAGE, 2);
        slotsLeft--;
        // anything left: extra bows (each its own slot) to take the last slots
        while (slotsLeft-- > 0) g.p.inv.add(itemWpn(W_BOW), 1);
    };
    fillWithJunk();
    check(g.p.bagUsed() == g.p.bagCap(), "test setup: every slot is in use");
    {
        g.animals.clear();
        Animal* a = g.spawnAnimal(A_DEER, g.p.pos + V2(10, 0), 0, 0);
        g.killAnimal(*a);
        Animal& c = g.animals[0];
        int before = g.p.mat(M_MEAT);
        c.remain[M_MEAT] = 5;
        c.totalRemain += 5;
        int remBefore = c.remain[M_MEAT];
        g.bagWarn = 0;
        const int64_t xpFull = g.p.sk.s[SK_BUTCHERING].xp;
        g.gatherStep(c);
        check(g.p.mat(M_MEAT) == before && c.remain[M_MEAT] == remBefore, "gathering is refused when the bag is full and no stack can take it");
        check(g.p.sk.s[SK_BUTCHERING].xp == xpFull, "a refused (bag full) carcass step earns no XP");
        g.p.inv.remove(itemMat(M_TUSK), 50);  // frees one slot
        g.gatherStep(c);
        check(g.p.mat(M_MEAT) >= before + 1, "after freeing a slot, gathering works again");
        check(g.p.sk.s[SK_BUTCHERING].xp > xpFull, "...and cutting meat trains Butchering");
        // a partly filled meat stack can still take more in a full bag
        g.p.inv.clear();
        fillWithJunk();
        g.p.inv.remove(itemMat(M_TUSK), 50);
        g.p.inv.add(MEAT, 10);
        int free0 = g.p.inv.freeSlots();
        c.remain[M_MEAT] = 3;
        g.gatherStep(c);
        check(free0 == 0 && g.p.mat(M_MEAT) > 10, "a full bag still accepts items that fit an existing stack");
    }
    // the recipe table grew: find the original recipes by what they make instead of by position
    auto findRecipe = [&](int kind, int out, int firstMat) {
        for (int r = 0; r < RECIPE_N; r++)
            if (RECIPES[r].kind == kind && RECIPES[r].out == out && RECIPES[r].mat[0] == firstMat) return r;
        return -1;
    };
    const int R_BONE = findRecipe(RK_WEAPON, W_BONE, M_BONE), R_ARROW = findRecipe(RK_AMMO, AM_ARROW, M_BONE),
              R_BOLT = findRecipe(RK_AMMO, AM_BOLT, M_BONE), R_BANDAGE = findRecipe(RK_BANDAGE, 0, M_HIDE);
    check(R_BONE == 0 && R_ARROW > 0 && R_BOLT > 0 && R_BANDAGE > 0, "the original recipes are all still in the table");
    {   // crafting in a full bag
        g.p.pos = g.world.keeper + V2(-8, 14);  // bone knives are made at the shop
        g.p.inv.clear();
        fillWithJunk();
        g.p.inv.remove(itemMat(M_TUSK), 50);
        g.p.inv.remove(itemMat(M_ANTLER), 50);
        g.p.inv.remove(itemWpn(W_BONE), 1);  // three free slots: bone, hide and a bit of meat fill them, so the bag is full again
        g.p.inv.add(BONE, 3);
        g.p.inv.add(HIDE, 1);
        g.p.inv.add(MEAT, 1);
        int bonesBefore = g.p.mat(M_BONE);
        bool ok = g.craft(R_BONE);  // BONE KNIFE x4: needs 3 bone + 1 hide, the two ingredient stacks vanish, the knives take a slot
        check(ok && g.p.wpn(W_BONE) == 4 && g.p.mat(M_BONE) == bonesBefore - 3, "crafting in a full bag works when the ingredients free up the slot it needs");
        g.p.inv.clear();
        fillWithJunk();
        g.p.inv.remove(itemMat(M_TUSK), 50);
        g.p.inv.remove(itemMat(M_ANTLER), 50);
        g.p.inv.remove(itemWpn(W_BONE), 1);
        g.p.inv.add(BONE, 10);  // the free slots go to bone, hide and meat; the recipe uses only part of the bone and hide stacks,
        g.p.inv.add(HIDE, 5);   // so nothing is freed and the finished knives have nowhere to go
        g.p.inv.add(MEAT, 1);
        int free1 = g.p.inv.freeSlots();
        int b0 = g.p.mat(M_BONE), h0 = g.p.mat(M_HIDE);
        bool ok2 = g.craft(R_BONE);
        check(free1 == 0 && !ok2 && g.p.mat(M_BONE) == b0 && g.p.mat(M_HIDE) == h0 && g.p.wpn(W_BONE) == 0, "crafting with no room is refused and the ingredients are returned untouched");
    }
    {   // picking up thrown weapons
        g.p.inv.clear();
        fillWithJunk();
        g.p.inv.remove(itemWpn(W_JAVELIN), 1);  // free exactly one slot, then take it with another item
        g.p.inv.add(MEAT, 1);
        g.items.clear();
        g.items.push_back({g.p.pos, W_JAVELIN, 0.f, 0, -1});
        for (int i = 0; i < 5; i++) g.update(1.f / 60.f);
        check(g.items.size() == 1 && g.p.wpn(W_JAVELIN) == 0, "a thrown weapon stays on the ground while the bag is full");
        g.p.inv.remove(MEAT, 1);
        for (int i = 0; i < 5; i++) g.update(1.f / 60.f);
        check(g.items.empty() && g.p.wpn(W_JAVELIN) == 1, "and is picked up as soon as there is room");
    }
    {   // backpack upgrades only add slots
        g.newGame();
        g.state = ST_PLAY;
        g.p.coins = 5000;
        g.p.inv.add(HIDE, 50);
        g.p.inv.add(itemMat(M_FUR), 50);
        g.p.inv.add(BONE, 50);
        g.p.inv.add(itemMat(M_TUSK), 50);
        int used0 = g.p.bagUsed(), meat0 = g.p.mat(M_HIDE);
        bool u1 = g.buyUpgrade(U_BAG);
        check(u1 && g.p.bagCap() == 30 && item(HIDE).stack == 99, "backpack level 1: 30 slots, stack size unchanged");
        bool u2 = g.buyUpgrade(U_BAG);
        check(u2 && g.p.bagCap() == 40, "backpack level 2: 40 slots");
        check(g.p.bagUsed() >= used0 - 1 && g.p.mat(M_HIDE) <= meat0, "the upgrade kept the items (minus its own material cost)");
        check(!g.buyUpgrade(U_BAG), "no third backpack level");
    }
    // ---- inventory screen (key I) and crafting
    printf("inventory screen and crafting\n");
    {
        auto frame = [&](int n = 1) {
            for (int i = 0; i < n; i++) {
                g.update(1.f / 60.f);
                g.in.lPressed = g.in.lReleased = g.in.ePressed = g.in.escPressed = g.in.enterPressed = g.in.tabPressed = false;
                g.in.invToggle = false;
                g.in.selectKey = -1;
            }
        };
        auto shotDir = getenv("INV_SHOTS");
        auto snap = [&](const char* name) {
            g.render();
            if (shotDir) g.saveCanvas((std::string(shotDir) + "/" + name + ".bmp").c_str());
        };
        g.newGame();
        g.state = ST_PLAY;
        g.helpT = 0;
        g.animals.clear();
        g.items.clear();
        g.p.pos = g.world.spawn;
        // ---- open / close
        g.in.invToggle = true;
        frame();
        check(g.state == ST_INVENTORY, "pressing I opens the inventory screen");
        g.in.invToggle = true;
        frame();
        check(g.state == ST_PLAY, "pressing I again closes it");
        g.in.invToggle = true;
        frame();
        g.in.escPressed = true;
        frame();
        check(g.state == ST_PLAY, "ESC also closes it (and does not open the pause menu)");
        // can't open while charging a throw
        g.p.charging = true;
        g.in.invToggle = true;
        frame();
        check(g.state == ST_PLAY, "it does not open in the middle of a throw");
        g.p.charging = false;
        // ---- time stands still
        g.animals.clear();
        Animal* w = g.spawnAnimal(A_DEER, g.p.pos + V2(60, 0), 0, 0);
        w->st = AS_WANDER;
        w->idleT = 0;
        w->target = w->pos + V2(80, 0);
        g.in.invToggle = true;
        frame();
        float day0 = g.dayT;
        V2 apos = g.animals[0].pos;
        frame(120);
        check(g.dayT == day0 && g.animals[0].pos.x == apos.x && g.animals[0].pos.y == apos.y, "the world is paused while the pack is open (clock and animals do not move)");
        // ---- cursor movement
        g.p.inv.clear();
        g.p.inv.add(itemWpn(W_FLINT), 8);
        g.p.inv.add(itemWpn(W_BONE), 3);
        g.p.inv.add(itemWpn(W_BOW), 1);
        g.p.inv.add(itemAmmo(AM_ARROW), 12);
        g.p.inv.add(ITEM_CONS0 + C_BANDAGE, 2);
        g.invSel = 0;
        auto press = [&](bool* key) {
            *key = true;
            frame();
            *key = false;
            frame();
        };
        press(&g.in.right);
        press(&g.in.right);
        check(g.invSel == 2, "right arrow moves the cursor one slot per key press");
        g.in.right = true;
        frame(30);
        g.in.right = false;
        check(g.invSel == 3, "holding a key does not make the cursor run away (one step, not repeating)");
        press(&g.in.down);
        check(g.invSel == 11, "down moves one row (8 columns)");
        for (int i = 0; i < 12; i++) press(&g.in.left);
        check(g.invSel % 8 == 0, "the cursor stops at the left edge");
        for (int i = 0; i < 12; i++) press(&g.in.down);
        check(g.invSel / 8 == 2 && g.invSel < 20, "the cursor stops at the last row of a 20-slot pack");
        // ---- equip / use with Enter
        g.invSel = 1;  // bone knives
        g.in.enterPressed = true;
        frame();
        check(g.p.sel == W_BONE, "ENTER on a weapon equips it");
        g.invSel = 2;
        g.in.enterPressed = true;
        frame();
        check(g.p.sel == W_BOW, "ENTER on the bow equips it");
        g.p.sel = W_FLINT;
        g.invSel = 3;
        g.in.enterPressed = true;
        frame();
        check(g.p.sel == W_BOW, "ENTER on arrows equips the bow that fires them");
        g.p.inv.remove(itemWpn(W_BOW), 1);
        g.p.sel = W_FLINT;
        g.in.enterPressed = true;
        frame();
        check(g.p.sel == W_FLINT, "arrows with no bow do nothing");
        g.p.hp = g.p.maxhp;
        g.invSel = 4;
        int b0 = g.p.bandages();
        g.in.enterPressed = true;
        frame();
        check(g.p.bandages() == b0 && g.p.hp == g.p.maxhp, "a bandage is not wasted at full health");
        g.p.hp = 30;
        g.in.enterPressed = true;
        frame();
        check(g.p.bandages() == b0 - 1 && g.p.hp == 30 + BANDAGE_HEAL, "ENTER on a bandage heals 45 HP and uses one");
        // ---- mouse: click a slot
        g.in.mx = 24 + 8 + 5 * 21 + 8;  // slot 5, as laid out by drawInventory
        g.in.my = 12 + 38 + 2 + 8;
        g.in.uiClick = true;
        g.render();
        check(g.invSel == 5, "clicking a slot selects it");
        g.in.uiClick = false;
        // ---- sort
        g.p.inv.clear();
        g.p.inv.slots[0] = {MEAT, 30};  // partial stacks as they can appear in an old or edited save
        g.p.inv.slots[1] = {HIDE, 5};
        g.p.inv.slots[3] = {MEAT, 20};
        int usedBefore = g.p.bagUsed();
        g.p.inv.compact();
        check(usedBefore == 3 && g.p.bagUsed() == 2 && g.p.mat(M_MEAT) == 50 && g.p.mat(M_HIDE) == 5 && g.p.inv.slots[2].empty(), "SORT merges split stacks to the front without losing anything");
        // ---- screenshots of the screens (only written when INV_SHOTS is set)
        g.p.inv.clear();
        g.p.inv.add(itemWpn(W_FLINT), 8);
        g.p.inv.add(itemWpn(W_JAVELIN), 4);
        g.p.inv.add(itemWpn(W_BOW), 1);
        g.p.inv.add(itemWpn(W_CROSSBOW), 1);
        g.p.inv.add(itemAmmo(AM_ARROW), 24);
        g.p.inv.add(itemAmmo(AM_BOLT), 6);
        g.p.inv.add(ITEM_CONS0 + C_BANDAGE, 3);
        for (int m = 0; m < M_CARCASS_N; m++) g.p.inv.add(itemMat(m), 3 + m * 9);
        g.p.sel = W_BOW;
        g.invTab = 0;
        g.invSel = 2;
        g.in.mx = 24 + 8 + 4 * 21 + 8;
        g.in.my = 12 + 38 + 2 + 8;
        snap("inv_backpack");
        g.invTab = 1;
        g.in.mx = 5;
        g.in.my = 5;
        snap("inv_crafting");
        g.invTab = 0;
        g.p.up[U_BAG] = 2;
        g.p.syncPack();
        g.invSel = 30;
        snap("inv_backpack_40");
        g.p.up[U_BAG] = 0;
        g.p.syncPack();
        // ---- crafting from the screen
        g.invTab = 1;
        g.p.inv.clear();
        g.p.inv.add(BONE, 5);
        g.p.inv.add(itemMat(M_FEATHER), 4);
        g.p.inv.add(itemMat(M_HORN), 1);
        g.p.inv.add(HIDE, 4);
        g.p.inv.add(itemMat(M_ORGAN), 2);
        g.p.inv.add(itemMat(M_TUSK), 1);
        g.p.inv.add(itemMat(M_ANTLER), 1);
        g.p.inv.add(itemMat(M_FUR), 1);
        auto clickRecipe = [&](int r) {  // click the CRAFT button of recipe r: pick its category filter, then its row in that list
            g.craftCat = RECIPES[r].cat;
            g.craftScroll = 0;
            g.craftOnly = false;
            int row = 0;
            for (int q = 0; q < r; q++)
                if (RECIPES[q].cat == RECIPES[r].cat) row++;
            g.in.mx = 24 + 8 + (352 - 16 - 5) - 54 + 25;
            g.in.my = 12 + 38 + 14 + row * 22 + 4 + 6;
            g.in.uiClick = true;
            g.render();
            g.in.uiClick = false;
        };
        g.p.pos = g.world.spawn + V2(300, 300);  // far from the shop
        check(!g.nearShop(), "test setup: the hunter is away from the shop");
        clickRecipe(R_BONE);
        check(g.p.wpn(W_BONE) == 0 && g.p.mat(M_BONE) == 5, "weapon recipes are refused away from the shop");
        clickRecipe(R_ARROW);
        check(g.p.ammo(AM_ARROW) == 6 && g.p.mat(M_BONE) == 4 && g.p.mat(M_FEATHER) == 2, "ARROWS x6 can be crafted anywhere: 1 bone + 2 feathers");
        clickRecipe(R_BOLT);
        check(g.p.ammo(AM_BOLT) == 4 && g.p.mat(M_HORN) == 0 && g.p.mat(M_BONE) == 2, "BOLTS x4: 2 bone + 1 horn");
        clickRecipe(R_BOLT);
        check(g.p.ammo(AM_BOLT) == 4 && g.p.mat(M_BONE) == 2, "a recipe without its ingredients does nothing");
        clickRecipe(R_BANDAGE);
        check(g.p.bandages() == 2 && g.p.mat(M_HIDE) == 2 && g.p.mat(M_ORGAN) == 1, "BANDAGE x2: 2 hide + 1 organs");
        // bandages stack to 9: with a free slot the extra ones start a second stack; with a full bag only 1 more fits, so the recipe is refused
        g.p.inv.set(ITEM_CONS0 + C_BANDAGE, 8);
        g.p.inv.add(HIDE, 4);
        g.p.inv.add(itemMat(M_ORGAN), 2);
        bool cr = g.craft(R_BANDAGE);
        check(cr && g.p.bandages() == 10, "with a free slot, bandages beyond a full stack of 9 go into a second stack");
        g.p.inv.clear();
        for (int i = 0; i < g.p.bagCap() - 3; i++) g.p.inv.add(itemWpn(W_BOW), 1);  // one slot per bow: nearly full
        g.p.inv.add(ITEM_CONS0 + C_BANDAGE, 8);
        g.p.inv.add(HIDE, 5);
        g.p.inv.add(itemMat(M_ORGAN), 3);
        int hide0 = g.p.mat(M_HIDE), org0 = g.p.mat(M_ORGAN), fs = g.p.inv.freeSlots();
        bool cr2 = g.craft(R_BANDAGE);
        check(fs == 0 && !cr2 && g.p.bandages() == 8 && g.p.mat(M_HIDE) == hide0 && g.p.mat(M_ORGAN) == org0, "in a full bag the bandage recipe is refused and the ingredients are returned");
        // near the shop: weapons allowed
        g.p.pos = g.world.keeper + V2(-8, 14);
        g.p.inv.clear();
        g.p.inv.add(HIDE, 2);
        g.p.inv.add(BONE, 4);
        check(g.nearShop(), "test setup: the hunter is at the shop");
        clickRecipe(R_BONE);
        check(g.p.wpn(W_BONE) == 4, "at the shop the same screen crafts weapons");
        // ---- the quest about crafting a weapon only counts weapons
        g.questIdx = 4;
        g.refreshQuest();
        g.questActive = true;
        g.questProg = 0;
        g.p.inv.add(BONE, 6);
        g.p.inv.add(itemMat(M_FEATHER), 4);
        g.craft(R_ARROW);
        check(g.questProg == 0, "crafting arrows does not count for 'craft a weapon'");
        g.p.inv.add(HIDE, 2);
        g.craft(R_BONE);
        check(g.questProg == 1, "crafting a weapon does");
        // ---- every screen renders in every state without trouble
        bool rendered = true;
        for (int lvl = 0; lvl < 3; lvl++)
            for (int tab = 0; tab < 2; tab++)
                for (int fill = 0; fill < 2; fill++) {
                    g.p.up[U_BAG] = lvl;
                    g.p.syncPack();
                    g.p.inv.clear();
                    if (fill)
                        for (int i = 0; i < g.p.bagCap(); i++) g.p.inv.add(i < ITEM_COUNT ? i : itemWpn(W_BOW), 1);
                    g.invTab = tab;
                    g.invSel = g.p.bagCap() - 1;
                    g.state = ST_INVENTORY;
                    g.render();
                    rendered = rendered && g.state == ST_INVENTORY;
                }
        check(rendered, "all tabs render with 20 / 30 / 40 slots, empty and completely full");
        g.state = ST_PLAY;
    }
    // ---- saving
    printf("saving and migration\n");
    auto writeFile = [&](const std::string& text) {
        std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
        out << text;
    };
    auto removeAll = [&]() {
        std::remove(g.savePath().c_str());
        std::remove((g.savePath() + ".v1.bak").c_str());
        std::remove((g.savePath() + ".tmp").c_str());
    };
    removeAll();
    {   // v2 round trip, including a non-default backpack, partial stacks and several stacks of one item
        g.newGame();
        g.p.up[U_BAG] = 1;
        g.p.syncPack();
        g.p.inv.clear();
        g.p.inv.add(MEAT, 250);
        g.p.inv.add(itemAmmo(AM_ARROW), 37);
        g.p.inv.add(itemWpn(W_BOW), 1);
        g.p.inv.add(itemWpn(W_FLINT), 31);
        g.p.coins = 777;
        g.save();
        Player keep = g.p;
        g.newGame();
        bool ld = g.load();
        bool same = ld && g.loadReport.version == 2 && !g.loadReport.migrated && g.loadReport.dropped == 0 && g.p.coins == 777 && g.p.bagCap() == 30;
        for (int i = 0; i < ITEM_COUNT; i++) same = same && g.p.inv.count(i) == keep.inv.count(i);
        same = same && g.p.bagUsed() == keep.bagUsed();
        check(same, "v2 save / load round trip: coins, 30-slot pack and every item count identical");
        std::ifstream f(g.savePath());
        std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        check(all.find("version 2") != std::string::npos && all.find("slot raw_meat 99") != std::string::npos && all.find("slot hunting_bow 1") != std::string::npos, "the file is readable text: version line and \"slot <name> <count>\" lines");
        check(!std::ifstream(g.savePath() + ".tmp"), "no temporary file left behind by the write-then-rename save");
    }
    {   // v1 (current layout) migration
        removeAll();
        std::string v1 = "coins 321\nmats 1 2 3 4 5 6 7 8 9 10 11\nwpn 8 3 2 4 1 1\nammo 25 9\nup 2 1 0 1\nbandages 4\nday 99.5\nkills 17\nquest 2 1 3\ndisc 10100000000000\n";
        writeFile(v1);
        bool ld = g.load();
        bool same = ld && g.loadReport.migrated && g.loadReport.version == 1 && g.loadReport.dropped == 0 && g.p.coins == 321 && g.p.up[U_BAG] == 1 && g.p.bagCap() == 30 && g.p.up[U_TOOL] == 2 && g.kills == 17;
        const int exMat[M_COUNT] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
        for (int m = 0; m < M_COUNT; m++) same = same && g.p.mat(m) == exMat[m];
        const int exW[W_COUNT] = {8, 3, 2, 4, 1, 1};
        for (int w = 0; w < W_COUNT; w++) same = same && g.p.wpn(w) == exW[w];
        same = same && g.p.ammo(AM_ARROW) == 25 && g.p.ammo(AM_BOLT) == 9 && g.p.bandages() == 4;
        check(same, "a v1 save migrates: every material, weapon, ammo type, bandage, coin and upgrade preserved");
        std::ifstream b(g.savePath() + ".v1.bak");
        std::string bak((std::istreambuf_iterator<char>(b)), std::istreambuf_iterator<char>());
        check(bak == v1, "the original v1 file was backed up byte-for-byte as save.v1.bak");
        g.save();
        removeAll();
        check(true, "(cleanup)");
    }
    {   // oldest layout: 7 materials, 4 weapons, no ammo / quest / disc lines
        std::string old = "coins 50\nmats 3 2 1 0 0 0 0\nwpn 5 0 0 2\nup 0 0 0 0\nbandages 1\nday 10\nkills 2\n";
        writeFile(old);
        bool ld = g.load();
        bool ok = ld && g.p.mat(M_MEAT) == 3 && g.p.mat(M_HIDE) == 2 && g.p.mat(M_BONE) == 1 && g.p.wpn(W_FLINT) == 5 && g.p.wpn(W_JAVELIN) == 2 &&
                  g.p.wpn(W_BOW) == 0 && g.p.mat(M_FEATHER) == 0 && g.p.ammo(AM_ARROW) == 0 && g.p.bandages() == 1 && g.p.coins == 50;
        check(ok, "an early v1 save (7 materials, 4 weapons, no ammo line) migrates; the new items start at zero");
        removeAll();
    }
    {   // damaged v1: garbage, negative and enormous numbers
        writeFile("coins -5\nmats 4 abc 9 -7 999999999 1\nwpn 1000000 2\nammo\nbandages zz\nkills 3\n");
        bool ld = g.load();
        bool sane = ld && g.p.coins >= 0 && g.p.bagUsed() <= g.p.bagCap();
        for (int m = 0; m < M_COUNT; m++) sane = sane && g.p.mat(m) >= 0;
        sane = sane && g.p.mat(M_MEAT) == 4 && g.p.mat(M_BONE) == 0 && g.p.mat(M_ORGAN) == 0;  // "abc" stops that line; the rest stay zero
        check(sane, "a damaged v1 save loads without crashing, no negative counts, and never overfills the bag");
        check(g.loadReport.dropped > 0, "items that cannot fit (absurd counts) are reported as dropped instead of lost silently");
        removeAll();
    }
    {   // v2 with an item from the future, and more slots than the pack has
        std::string s2 = "version 2\ncoins 10\nup 0 0 0 0\nslot raw_meat 12\nslot unicorn_horn 3\nslot hide 4\n";
        for (int i = 0; i < 25; i++) s2 += std::string("slot hunting_bow 1\n");
        writeFile(s2);
        bool ld = g.load();
        check(ld && g.loadReport.unknown == 1 && g.p.mat(M_MEAT) == 12 && g.p.mat(M_HIDE) == 4, "an unknown item in a v2 file is skipped; the rest of the save loads");
        check(g.loadReport.dropped == 25 - (20 - 2) && g.p.bagUsed() == 20, "more stacks than the pack holds: what fits is kept, the rest is counted as dropped");
        removeAll();
    }
    {   // not a save at all
        writeFile("hello world\nthis is not a save\n");
        g.p.coins = 4242;
        bool ld = g.load();
        check(!ld && g.loadReport.version == 0, "a file that is not a save is rejected (load returns false)");
        removeAll();
        check(!g.load(), "no file: load returns false");
    }
    // ======================================================== gathering, crafting, structures
    printf("gathering, crafting and structures\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.helpT = 0;
        g.p.inv.clear();
        auto recipeFor = [&](int kind, int out, int firstMat, int count = -1) {
            for (int r = 0; r < RECIPE_N; r++)
                if (RECIPES[r].kind == kind && RECIPES[r].out == out && RECIPES[r].mat[0] == firstMat && (count < 0 || RECIPES[r].count == count)) return r;
            return -1;
        };
        // stand beside resource `rs` such that findHarvest picks exactly it
        auto standAt = [&](int rs, bool& node, int& idx, int skip = 0) {
            for (int pass = 0; pass < 2; pass++) {
                const bool wantNode = pass == 0;
                const size_t n = wantNode ? g.world.nodes.size() : g.world.props.size();
                for (size_t i = 0; i < n; i++) {
                    const int res = wantNode ? nodeRes(g.world.nodes[i].kind) : g.propRsId[i];
                    if (res != rs) continue;
                    const float x = wantNode ? g.world.nodes[i].x : g.world.props[i].x, y = wantNode ? g.world.nodes[i].y : g.world.props[i].y;
                    g.p.pos = V2(x, y + 12);
                    HTarget t;
                    if (g.findHarvest(t) && t.node == wantNode && t.idx == (int)i && skip-- <= 0) {
                        node = wantNode;
                        idx = (int)i;
                        return true;
                    }
                }
            }
            return false;
        };
        auto swingUntilGone = [&](bool node, int idx, int& steps) {
            steps = 0;
            while (!g.hstate(node, idx).gone && steps < 4000) {
                HTarget t;
                if (!g.findHarvest(t) || t.idx != idx || t.node != node) return false;
                g.harvestStep(t, 0.05f);
                steps++;
            }
            return g.hstate(node, idx).gone;
        };
        auto canHarvest = [&](int rs) {
            const ResDef& RD = RES[rs];
            return RD.tool < 0 || g.bestTool(RD.tool) >= RD.minTier;
        };
        bool node = false;
        int idx = -1, steps = 0;

        // ---- tool tiers gate ore (iron is found in the mountain map)
        g.travelTo(B_MOUNTAIN, MAPS[B_MOUNTAIN].entryTx, MAPS[B_MOUNTAIN].entryTy, false);
        g.travelCd = 0;
        check(standAt(RS_IRON, node, idx), "an iron vein exists and can be reached by line of sight");
        check(!canHarvest(RS_IRON), "iron cannot be mined with bare hands");
        g.p.inv.add(itemTool(TL_PICK1), 1);
        check(!canHarvest(RS_IRON), "a stone pickaxe (tier 1) is not enough for iron");
        g.p.inv.add(itemTool(TL_PICK2), 1);
        check(canHarvest(RS_IRON) && !canHarvest(RS_GOLD), "a copper pickaxe mines iron but not gold");
        check(g.bestTool(TT_PICK) == 2 && g.bestTool(TT_AXE) == 0, "bestTool picks the highest tier of the right type");
        // ---- swing, finish, reward, depletion
        const int ironBefore = g.p.mat(M_IRON);
        const bool done = swingUntilGone(node, idx, steps);
        check(done, "holding the swing finishes the vein");
        check(g.p.mat(M_IRON) > ironBefore, "finishing the vein pays out iron ore");
        check(g.p.sk.s[SK_MINING].xp == 2 + RES[RS_IRON].hits * 2 + RES[RS_IRON].minTier * 4, "finishing a vein earns Mining XP once, not per swing");
        const float minSecs = (RES[RS_IRON].hits - 1) * RES[RS_IRON].swing / TOOLDEF[TL_PICK2].speed;
        check(steps * 0.05f >= minSecs, "a vein takes at least hits x swing time (no free rewards)");
        {
            HTarget t;
            const bool still = g.findHarvest(t) && t.node == node && t.idx == idx;
            check(!still, "a depleted vein cannot be harvested again (no duplicate rewards)");
            const int had = g.p.mat(M_IRON);
            HTarget fake;
            fake.valid = true; fake.node = node; fake.idx = idx; fake.res = RS_IRON; fake.pos = g.p.pos;
            for (int i = 0; i < 200; i++) g.harvestStep(fake, 0.05f);
            check(g.p.mat(M_IRON) == had, "forcing swings at a depleted vein gives nothing");
        }
        // ---- respawn timing
        {
            for (float t = 0; t < RES[RS_IRON].respawn * 0.5f; t += 1.f) g.updateHarvest(1.f);
            check(g.hstate(node, idx).gone, "it is still depleted halfway through the respawn time");
            for (float t = 0; t < RES[RS_IRON].respawn * 0.7f + REGROW_FADE + 2.f; t += 1.f) g.updateHarvest(1.f);
            check(g.harvestUp(node, idx), "it has grown back after the respawn time");
            HTarget t;
            check(g.findHarvest(t) && t.idx == idx, "and can be harvested again");
        }
        g.travelTo(B_HOME, MAPS[B_MOUNTAIN].portals[0].arriveTx, MAPS[B_MOUNTAIN].portals[0].arriveTy, false);  // back to the home map for the trees and herbs
        g.travelCd = 0;
        // ---- trees: solid tile cleared while felled, restored when regrown, border trees stay
        {
            g.p.inv.add(itemTool(TL_AXE1), 1);
            check(standAt(RS_OAK, node, idx, 5), "an oak is reachable");
            const Prop& pr = g.world.props[idx];
            const int tx = (int)(pr.x / TS), ty = (int)(pr.y / TS);
            check(g.world.solid[ty][tx], "a standing oak is a solid tile");
            const int woodBefore = g.p.mat(M_WOOD);
            check(swingUntilGone(node, idx, steps), "an oak can be chopped down");
            check(g.p.mat(M_WOOD) > woodBefore, "felling an oak gives wood");
            check(g.p.sk.s[SK_WOODCUTTING].xp == 2 + RES[RS_OAK].hits * 2 + RES[RS_OAK].minTier * 4, "felling an oak earns Woodcutting XP once");
            check(!g.world.solid[ty][tx], "a felled tree no longer blocks the way");
            g.p.pos = V2(pr.x + 120, pr.y + 120);  // walk away: nobody blocks the regrowth
            for (float t = 0; t < RES[RS_OAK].respawn * 1.3f + REGROW_FADE + 5.f; t += 1.f) g.updateHarvest(1.f);
            check(g.harvestUp(node, idx) && g.world.solid[ty][tx], "the oak regrows and is solid again");
        }
        {
            int bad = 0, harvestable = 0;
            for (size_t i = 0; i < g.world.props.size(); i++) {
                const int tx = (int)(g.world.props[i].x / TS), ty = (int)(g.world.props[i].y / TS);
                if (g.propRsId[i] >= 0) {
                    harvestable++;
                    if (protectedTile(tx, ty, g.world.mapId)) bad++;
                }
            }
            check(harvestable > 0 && bad == 0, "trees on the map border and around the home core are never harvestable");
        }
        // ---- no gathering through obstacles
        {
            check(standAt(RS_HERB, node, idx), "a wild herb is reachable");
            const V2 target(g.world.nodes[idx].x, g.world.nodes[idx].y - 4);
            // put a solid wall tile between the hunter and the plant
            const int wx = (int)((g.p.pos.x + target.x) / 2 / TS), wy = (int)((g.p.pos.y + target.y) / 2 / TS);
            const bool had = g.world.solid[wy][wx];
            const int ptx = (int)(target.x / TS), pty = (int)(target.y / TS);
            if ((wx != ptx || wy != pty)) {
                g.world.solid[wy][wx] = true;
                g.p.pos = target + V2(0, 24);
                g.world.solid[(int)((g.p.pos.y + target.y) / 2 / TS)][(int)(target.x / TS)] = true;
                HTarget t;
                const bool seen = g.findHarvest(t) && t.node && t.idx == idx;
                check(!seen, "a solid tile between the hunter and the plant blocks gathering");
                g.world.solid[(int)((g.p.pos.y + target.y) / 2 / TS)][(int)(target.x / TS)] = had;
            }
            g.world.solid[wy][wx] = had;
        }
        // ---- a full bag never swallows a resource
        {
            check(standAt(RS_HERB, node, idx, 1), "another herb is reachable");
            g.p.inv.clear();
            for (int i = 0; i < g.p.bagCap(); i++) g.p.inv.add(itemWpn(W_BOW), 1);
            check(g.p.bagUsed() == g.p.bagCap(), "test setup: the bag is full");
            for (int i = 0; i < 200; i++) {
                HTarget t;
                if (!g.findHarvest(t)) break;
                g.harvestStep(t, 0.05f);
            }
            check(!g.hstate(node, idx).gone && g.p.mat(M_HERB) == 0, "with a full bag the plant stays and nothing is lost");
            g.p.inv.clear();
            check(swingUntilGone(node, idx, steps) && g.p.mat(M_HERB) > 0, "with room again the same plant gives its reward");
        }
        // ---- crafting rules
        g.p.inv.clear();
        g.structs.clear();
        {
            // a spot well away from the village: nothing is "near" a fire or the shop there
            bool found = false;
            for (int ty = 20; ty < MH - 20 && !found; ty += 5)
                for (int tx = 20; tx < MW - 20 && !found; tx += 5) {
                    g.p.pos = V2(tx * TS + 8.f, ty * TS + 8.f - 18.f);
                    g.p.dir = 0;
                    if (dist(g.p.pos, g.world.fire) > 150 && dist(g.p.pos, g.world.keeper) > 150 && g.canPlaceAt(tx, ty) && !g.world.solidTile(tx, ty - 2)) found = true;
                }
            check(found, "test setup: an open spot far from the village");
            const int rCamp = recipeFor(RK_ITEM, itemPlace(PL_CAMPFIRE), M_WOOD);
            const int rCopperBar = recipeFor(RK_ITEM, itemMat(M_COPPERBAR), M_COPPER);
            const int rRope = recipeFor(RK_ITEM, itemMat(M_ROPE), M_FIBER);
            const int rCuAxe = recipeFor(RK_ITEM, itemTool(TL_AXE2), M_WOOD);
            const int rArmor1 = recipeFor(RK_GEAR, U_ARMOR, M_HIDE, 1), rArmor2 = recipeFor(RK_GEAR, U_ARMOR, M_FUR, 2);
            const int rIronAxe = recipeFor(RK_ITEM, itemTool(TL_AXE3), M_WOOD);
            check(rCamp >= 0 && rCopperBar >= 0 && rRope >= 0 && rCuAxe >= 0 && rArmor1 >= 0 && rArmor2 >= 0 && rIronAxe >= 0, "the key recipes are all in the table");
            check(g.craftBlock(rCamp) == "MISSING MATERIALS", "a recipe without its materials says so");
            // every recipe in the table produces something real and names real ingredients
            int badRecipes = 0;
            for (int r = 0; r < RECIPE_N; r++) {
                const Recipe& R = RECIPES[r];
                bool ok = R.count > 0 && !recipeName(R).empty();
                int ing = 0;
                for (int k = 0; k < 3; k++)
                    if (R.mat[k] >= 0) {
                        ing++;
                        ok = ok && R.mat[k] < M_COUNT && R.n[k] > 0;
                    }
                ok = ok && ing > 0;
                if (R.kind != RK_GEAR && R.kind != RK_BANDAGE) ok = ok && recipeItem(R) > 0 && recipeItem(R) < ITEM_COUNT;
                if (!ok) {
                    badRecipes++;
                    printf("    bad recipe #%d\n", r);
                }
            }
            check(badRecipes == 0, "every recipe has valid ingredients and a real output");
            // each raw material that a recipe needs can be gathered, dropped or made
            bool producible[M_COUNT] = {false};
            for (int rs = 0; rs < RS_COUNT; rs++)
                for (int k = 0; k < 3; k++)
                    if (RES[rs].y[k].mat >= 0) producible[RES[rs].y[k].mat] = true;
            for (int a = 0; a < A_COUNT; a++)
                for (int m = 0; m < M_COUNT; m++)
                    if (ANIMAL[a].drop[m][1] > 0) producible[m] = true;
            for (int r = 0; r < RECIPE_N; r++)
                if (RECIPES[r].kind == RK_ITEM && item(RECIPES[r].out).kind == IK_MATERIAL) producible[item(RECIPES[r].out).ref] = true;
            int unmakeable = 0;
            for (int r = 0; r < RECIPE_N; r++)
                for (int k = 0; k < 3; k++)
                    if (RECIPES[r].mat[k] >= 0 && !producible[RECIPES[r].mat[k]]) {
                        unmakeable++;
                        printf("    recipe #%d needs %s, which nothing provides\n", r, MAT_NAME[RECIPES[r].mat[k]]);
                    }
            check(unmakeable == 0, "every ingredient of every recipe can actually be obtained");

            g.p.inv.add(itemMat(M_COPPER), 2);
            g.p.inv.add(itemMat(M_WOOD), 12);
            g.p.inv.add(itemMat(M_STONE), 6);
            check(g.craftBlock(rCopperBar) == "STAND BY A CAMPFIRE", "smelting away from a fire is refused");
            check(!g.craft(rCopperBar) && g.p.mat(M_COPPER) == 2, "...and consumes nothing");
            check(g.craft(rCamp) && g.p.inv.count(itemPlace(PL_CAMPFIRE)) == 1 && g.p.mat(M_STONE) == 3, "a campfire is crafted from wood and stone");
            check(g.placeStructure(PL_CAMPFIRE) && g.structs.size() == 1 && g.p.inv.count(itemPlace(PL_CAMPFIRE)) == 0, "the campfire is placed");
            check(g.nearFire(), "standing by the placed campfire counts as a fire");
            check(g.craft(rCopperBar) && g.p.mat(M_COPPERBAR) == 1 && g.p.mat(M_COPPER) == 0, "copper smelts into a bar at the fire");
            // rope, then the copper axe
            g.p.inv.add(itemMat(M_FIBER), 3);
            check(g.craft(rRope) && g.p.mat(M_ROPE) == 1 && g.p.mat(M_FIBER) == 0, "fibre twists into rope");
            check(g.p.mat(M_COPPERBAR) == 1, "(bar still in the bag)");
            g.p.inv.add(itemMat(M_COPPERBAR), 1);
            check(g.craft(rCuAxe) && g.p.inv.count(itemTool(TL_AXE2)) == 1, "a copper axe is crafted from wood, a copper bar and rope");
            g.p.inv.add(itemMat(M_COPPERBAR), 2);
            g.p.inv.add(itemMat(M_ROPE), 1);
            g.p.inv.add(itemMat(M_WOOD), 2);
            check(g.craftBlock(rCuAxe) == "YOU ALREADY HAVE ONE", "a second copy of a single tool is refused");
            // iron tools need the shop's forge
            g.p.inv.add(itemMat(M_IRONBAR), 2);
            check(g.craftBlock(rIronAxe) == "STAND AT THE SHOP", "iron tools need the merchant");
            // armor order
            g.p.inv.add(itemMat(M_FUR), 4);
            g.p.inv.add(itemMat(M_HIDE), 12);
            g.p.inv.add(itemMat(M_FIBER), 2);
            check(g.craftBlock(rArmor2) == "MAKE THE PREVIOUS LEVEL FIRST", "armor level 2 needs level 1 first");
            check(g.craft(rArmor1) && g.p.up[U_ARMOR] == 1, "hide armor is worn at once");
            check(g.craft(rArmor2) && g.p.up[U_ARMOR] == 2, "then the second level");
            check(g.craftBlock(rArmor1) == "YOU WEAR BETTER ALREADY" || g.craftBlock(rArmor1) == "MISSING MATERIALS", "an older armor level cannot be crafted again");
            // consumables
            g.p.inv.clear();
            g.p.hp = g.p.maxhp - 50;
            g.p.inv.add(itemCons(C_SALVE), 1);
            g.p.inv.add(itemCons(C_BREW), 1);
            g.p.inv.add(itemCons(C_CACTUSWATER), 1);
            const float h0 = g.p.hp;
            check(g.useConsumable(C_SALVE) && g.p.hp > h0 && g.p.inv.count(itemCons(C_SALVE)) == 0, "a salve heals and is used up");
            g.p.hp = g.p.maxhp;
            check(!g.useConsumable(C_TONIC), "using something you do not own does nothing");
            check(g.useConsumable(C_BREW) && g.p.warmT > 100.f, "a warm brew gives warmth");
            check(g.useConsumable(C_CACTUSWATER) && g.p.coolT > 60.f && g.p.stamina >= 99.f, "cactus water refreshes");
            // chest: move items both ways, keeps the stack rules
            g.p.inv.clear();
            g.p.inv.add(itemMat(M_WOOD), 5);
            g.p.inv.add(itemMat(M_WOOD), 0);
            g.p.inv.add(itemPlace(PL_CHEST), 1);
            check(g.placeStructure(PL_CHEST) && g.structs.size() == 2, "a chest is placed");
            g.chestOpen = 1;
            int woodSlot = -1;
            for (int i = 0; i < g.p.bagCap(); i++)
                if (g.p.inv.slots[i].item == itemMat(M_WOOD)) woodSlot = i;
            check(woodSlot >= 0 && g.chestMove(1, woodSlot) && g.p.mat(M_WOOD) == 0 && g.structs[1].inv.count(itemMat(M_WOOD)) == 5, "wood moves from the pack into the chest");
            // save / load: depleted resources, structures and chest contents come back
            check(standAt(RS_HERB, node, idx, 2) && swingUntilGone(node, idx, steps), "(setup) one more plant is picked");
            g.save();
            const size_t nStructs = g.structs.size();
            g.structs.clear();
            g.resetHarvest();
            check(g.harvestUp(node, idx), "(setup) after a reset the plant is back");
            check(g.load(), "the save loads");
            check(g.structs.size() == nStructs, "placed structures survive a save / load");
            bool chestOk = false;
            for (auto& s : g.structs)
                if (s.kind == PL_CHEST && s.inv.count(itemMat(M_WOOD)) == 5) chestOk = true;
            check(chestOk, "chest contents survive a save / load");
            check(g.hstate(node, idx).gone, "a picked plant is still waiting to regrow after a save / load");
        }
    }
    printf("consumables and skill-gated crafting\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.p.inv.clear();
        auto cons = [&](int c) { g.p.inv.add(itemCons(c), 1); };
        g.p.maxhp = 200;
        g.p.hp = 20;
        cons(C_POULTICE);
        check(g.useConsumable(C_POULTICE) && (int)g.p.hp == 80 && g.p.inv.count(itemCons(C_POULTICE)) == 0, "a poultice heals 60 and is used up");
        cons(C_MEDICINE);
        check(g.useConsumable(C_MEDICINE) && (int)g.p.hp == 200, "herbal medicine heals 120 (capped at max health)");
        cons(C_POULTICE);
        check(!g.useConsumable(C_POULTICE) && g.p.inv.count(itemCons(C_POULTICE)) == 1, "healing items are refused (and kept) at full health");
        g.p.hp = 100;
        g.p.stamina = 5.f;
        g.p.exhausted = true;
        cons(C_JERKY);
        check(g.useConsumable(C_JERKY) && (int)g.p.hp == 120 && g.p.stamina >= 99.f && !g.p.exhausted, "dried meat heals 20 and restores stamina");
        g.p.stamina = 5.f;
        cons(C_STEW);
        check(g.useConsumable(C_STEW) && (int)g.p.hp == 200 && g.p.stamina >= 99.f, "hunter's stew heals 85 and restores stamina");
        g.p.hp = 50;
        cons(C_VIGOR);
        check(g.useConsumable(C_VIGOR) && (int)g.p.hp == 105 && g.p.swiftT > 85.f, "velvet tonic heals 55 and grants a speed boost");
        cons(C_NIGHTSIGHT);
        check(g.useConsumable(C_NIGHTSIGHT) && g.p.nightT > 295.f, "nightsight tonic starts a five-minute timer");
        cons(C_NIGHTSIGHT);
        check(!g.useConsumable(C_NIGHTSIGHT) && g.p.inv.count(itemCons(C_NIGHTSIGHT)) == 1, "a second nightsight tonic is refused while one is active (nothing wasted)");
        // skill gating
        auto find = [&](int kind, int out, int firstMat, int count) {
            for (int r = 0; r < RECIPE_N; r++)
                if (RECIPES[r].kind == kind && RECIPES[r].out == out && RECIPES[r].mat[0] == firstMat && RECIPES[r].count == count) return r;
            return -1;
        };
        const int rPoul = find(RK_ITEM, itemCons(C_POULTICE), M_EXTRACT, 2);
        check(rPoul >= 0, "(setup) the poultice recipe exists");
        g.newGame();
        g.state = ST_PLAY;
        g.p.inv.clear();
        g.p.inv.add(itemMat(M_EXTRACT), 4);
        g.p.inv.add(itemMat(M_FIBER), 4);
        check(g.craftBlock(rPoul) == "NEEDS ALCHEMY 3", "a recipe above your skill level says which skill and level it needs");
        check(!g.craft(rPoul) && g.p.mat(M_EXTRACT) == 4 && g.p.mat(M_FIBER) == 4 && g.p.inv.count(itemCons(C_POULTICE)) == 0, "...and a locked recipe consumes nothing");
        g.p.sk.s[SK_ALCHEMY].xp = xpForLevel(1) + xpForLevel(2);  // exactly level 3
        check(g.p.sk.level(SK_ALCHEMY) == 3 && g.craftBlock(rPoul).empty(), "reaching the level unlocks it");
        const int64_t before = g.p.sk.s[SK_ALCHEMY].xp;
        check(g.craft(rPoul) && g.p.inv.count(itemCons(C_POULTICE)) == 2 && g.p.mat(M_EXTRACT) == 3 && g.p.mat(M_FIBER) == 3, "crafting delivers the result and spends the ingredients");
        check(g.p.sk.s[SK_ALCHEMY].xp == before + recipeXp(RECIPES[rPoul]), "a successful craft earns the recipe's XP in the recipe's skill");
        g.p.inv.clear();
        const int64_t b2 = g.p.sk.s[SK_ALCHEMY].xp;
        check(!g.craft(rPoul) && g.p.sk.s[SK_ALCHEMY].xp == b2, "a failed craft earns no XP");
    }
    printf("gear effects and recipe audit\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.p.inv.clear();
        float dr = 1, rg = 1;
        g.curBiome = B_DESERT;
        g.dayT = 0.5f;
        g.climateMods(dr, rg);
        const float plainHeat = dr;
        g.p.inv.add(itemCarry(CG_DESERTWRAPS), 1);
        g.climateMods(dr, rg);
        check(plainHeat > 1.f && dr < plainHeat && dr >= 1.f, "desert wraps cut the heat drain while carried");
        g.p.inv.clear();
        g.curBiome = B_SNOW;
        g.p.warmT = 0;
        g.climateMods(dr, rg);
        const float cold = dr;
        g.p.inv.add(itemCarry(CG_WOLFCLOAK), 1);
        g.climateMods(dr, rg);
        const float cloak = dr;
        g.p.inv.add(itemCarry(CG_PARKA), 1);
        g.climateMods(dr, rg);
        check(cold > cloak && cloak > dr && dr == 1.f, "the wolf cloak halves the cold and the parka removes it");
        g.p.inv.clear();
        g.curBiome = B_MOUNTAIN;
        g.climateMods(dr, rg);
        const float thin = dr;
        g.p.inv.add(itemCarry(CG_HARNESS), 1);
        g.climateMods(dr, rg);
        check(dr < thin, "the climbing harness eases thin air");
        // snake wraps: only snake bites are softened
        g.p.inv.clear();
        g.p.hp = g.p.maxhp = 200;
        g.hurtPlayer(20, V2(0, 0), A_RATTLER);
        const float plain = 200 - g.p.hp;
        g.p.hp = 200;
        g.p.invuln = 0;
        g.p.inv.add(itemCarry(CG_VENOMWRAPS), 1);
        g.hurtPlayer(20, V2(0, 0), A_RATTLER);
        const float wrapped = 200 - g.p.hp;
        g.p.hp = 200;
        g.p.invuln = 0;
        g.hurtPlayer(20, V2(0, 0), A_WOLF);
        check(wrapped < plain && 200 - g.p.hp == plain, "venom wraps soften snake bites but not other attacks");
        // gear tiers
        g.p.inv.clear();
        bool tiers = true;
        for (int u = 0; u < 4; u++) tiers = tiers && std::string(gearName(u, UP_MAX[u])).size() > 0;
        check(tiers, "every gear line's top level has a name");
        g.newGame();
        g.state = ST_PLAY;
        g.p.up[U_BAG] = 2;
        g.p.syncPack();
        g.p.sk.s[SK_LEATHER].xp = SkillState::capXp();
        g.p.inv.add(itemMat(M_LEATHER), 8);
        g.p.inv.add(itemMat(M_ROPE), 4);
        g.p.inv.add(itemMat(M_MARMOTPELT), 3);
        int rPack = -1;
        for (int r = 0; r < RECIPE_N; r++)
            if (RECIPES[r].kind == RK_GEAR && RECIPES[r].out == U_BAG && RECIPES[r].count == 3) rPack = r;
        g.p.sk.s[SK_LEATHER].xp = 0;
        check(rPack >= 0 && g.craftBlock(rPack).find("NEEDS LEATHERWORK") == 0, "the explorer pack needs leatherwork skill");
        g.p.sk.s[SK_LEATHER].xp = SkillState::capXp();
        g.p.pos = g.world.fire;  // the recipe needs a campfire
        check(g.craft(rPack) && g.p.up[U_BAG] == 3 && g.p.inv.capacity() == PACK_SLOTS[3], "crafting the explorer pack gives 50 slots at once");
        // audit
        bool valid = true, skilled = true;
        bool used[M_COUNT] = {};
        for (int r = 0; r < RECIPE_N; r++) {
            const Recipe& R = RECIPES[r];
            valid = valid && R.count > 0 && !recipeName(R).empty();
            skilled = skilled && R.skill >= 0 && R.skill < SK_COUNT && R.minLv >= 1 && R.minLv <= MAX_LEVEL;
            for (int i = 0; i < 3; i++)
                if (R.mat[i] >= 0) {
                    valid = valid && R.mat[i] < M_COUNT && R.n[i] > 0;
                    used[R.mat[i]] = true;
                }
        }
        check(valid && skilled, "every recipe has a name, a count, valid ingredients and a valid skill level");
        bool trophiesUsed = true;
        for (int m = M_TROPHY0; m < M_TROPHY0 + A_COUNT; m++)
            if (!used[m]) {
                trophiesUsed = false;
                printf("    %s is used by no recipe\n", MAT_NAME[m]);
            }
        check(trophiesUsed, "every animal's exclusive material is an ingredient in at least one recipe");
        bool compUsed = true;
        for (int m : {M_PLANK, M_LEATHER, M_BONEFRAG, M_EXTRACT, M_GREASE, M_FAT})
            if (!used[m]) {
                compUsed = false;
                printf("    %s is used by no recipe\n", MAT_NAME[m]);
            }
        check(compUsed, "every processed material is consumed by a recipe");
        bool biomeOk = true;
        for (int w = W_FANGDAGGER; w < W_COUNT; w++) biomeOk = biomeOk && (WPN[w].biome < 0 || WPN[w].biomeMul > 1.f);
        check(biomeOk, "every biome weapon has a damage bonus for its biome");
    }
    printf("separate maps and travel\n");
    {
        auto packTotal = [&]() {
            long n = 0;
            for (auto& sl : g.p.inv.slots) n += sl.empty() ? 0 : sl.n;
            return n;
        };
        auto spotOk = [&]() {
            const V2 q = g.p.pos;
            return !g.world.blocked(q.x, q.y, 5.f) && q.x > 2 * TS && q.y > 2 * TS && q.x < (MW - 2) * TS && q.y < (MH - 2) * TS;
        };
        check(MW == 240 && MH == 168 && CW == 88 && CH == 60, "every map is 240x168 tiles; the original 88x60 Wildwood area sits inside the home map");
        bool allSame = true, allWalled = true, pure = true;
        for (int m = 0; m < B_COUNT; m++) {
            g.loadMapWorld(m);
            allSame = allSame && g.world.mapId == m && g.world.pixW() == 240 * TS && g.world.pixH() == 168 * TS;
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++) {
                    pure = pure && g.world.biome[y][x] == m;
                    if ((x < 2 || y < 2 || x >= MW - 2 || y >= MH - 2) && !g.world.solidTile(x, y) && !portalAt(m, x, y)) allWalled = false;
                }
        }
        check(allSame && pure, "all five maps have the same pixel size and only their own biome");
        check(allWalled, "every map's edge wall is closed except at its gates");
        g.loadMapWorld(B_HOME);
        bool homeSame = true;  // the same seed always builds the same map
        {
            World a, b;
            a.generate(WORLD_SEED, B_DESERT);
            b.generate(WORLD_SEED, B_DESERT);
            homeSame = a.props.size() == b.props.size() && a.nodes.size() == b.nodes.size() && memcmp(a.tile, b.tile, sizeof a.tile) == 0;
        }
        check(homeSame, "a map is rebuilt identically every time it is entered");
        // ---- travel keeps the hunter, and each map keeps its own state
        g.newGame();
        g.state = ST_PLAY;
        g.p.coins = 500;
        g.p.inv.add(itemMat(M_WOOD), 7);
        g.p.inv.add(itemMat(M_STONE), 5);
        g.p.inv.add(itemPlace(PL_CAMPFIRE), 2);
        g.p.hp = 55;
        g.p.sk.s[SK_HUNTING].xp = 300;
        g.questIdx = 2;
        g.dayT = 77;
        const long packBefore = packTotal();
        check(g.world.mapId == B_HOME && g.placeStructure(PL_CAMPFIRE) && g.structs.size() == 1, "(setup) a campfire is placed in the home map");
        const int homeTx = g.structs[0].tx, homeTy = g.structs[0].ty;
        g.p.inv.add(itemPlace(PL_CAMPFIRE), 0);
        const long packMid = packTotal();
        check(g.travelTo(B_DESERT, MAPS[B_DESERT].entryTx, MAPS[B_DESERT].entryTy, false) && g.world.mapId == B_DESERT && g.curBiome == B_DESERT, "travelling loads the desert map");
        check(spotOk(), "the hunter arrives on a free tile inside the map");
        check(packTotal() == packMid && g.p.coins == 500 && (int)g.p.hp == 55 && g.p.sk.s[SK_HUNTING].xp == 300 && g.questIdx == 2 && g.dayT == 77.f, "pack, coins, health, skills, quest and time are unchanged by a trip");
        check(g.structs.empty(), "the home campfire is not in the desert (maps do not mix)");
        g.travelCd = 0;
        check(g.placeStructure(PL_CAMPFIRE) && g.structs.size() == 1, "(setup) a campfire is placed in the desert");
        const int dTx = g.structs[0].tx, dTy = g.structs[0].ty;
        const long packNow = packTotal();  // (the placed campfire left the pack)
        check(g.world.landmarks.size() == 4 && g.landmarksFound() == 0, "the desert has its own four landmarks, none found yet");
        g.world.landmarks[1].found = true;
        int herb = -1;
        for (size_t i = 0; i < g.world.nodes.size(); i++)
            if (nodeRes(g.world.nodes[i].kind) >= 0) {
                herb = (int)i;
                break;
            }
        check(herb >= 0, "(setup) the desert has a gatherable node");
        g.hstate(true, herb).gone = true;
        g.hstate(true, herb).t = 500;
        g.hActive.push_back((1 << 28) | herb);
        g.items.push_back({g.p.pos + V2(20, 0), W_FLINT, 0.f, 0.f});
        const V2 hubPos = g.world.hub[B_DESERT];
        g.travelTo(B_HOME, MAPS[B_DESERT].portals[0].arriveTx, MAPS[B_DESERT].portals[0].arriveTy, false);
        check(g.world.mapId == B_HOME && spotOk(), "the way back leads home to a valid spot");
        check(g.structs.size() == 1 && g.structs[0].tx == homeTx && g.structs[0].ty == homeTy, "the home map still has only its own campfire");
        check(g.items.empty() && g.landmarksFound() == 0 && g.world.landmarks.empty(), "the desert's dropped weapon and landmarks are not in the home map");
        g.travelCd = 0;
        g.travelTo(B_DESERT, MAPS[B_DESERT].entryTx, MAPS[B_DESERT].entryTy, false);
        check(g.structs.size() == 1 && g.structs[0].tx == dTx && g.structs[0].ty == dTy && g.world.solid[dTy][dTx], "returning to the desert restores its campfire (and it blocks the tile)");
        check(g.hstate(true, herb).gone && g.landmarksFound() == 1 && g.world.landmarks[1].found, "...its harvested resource and its discovered landmark");
        check(g.items.size() == 1 && g.items[0].wpn == W_FLINT, "...and the weapon that lay on the ground");
        check(packTotal() == packNow && g.world.hub[B_DESERT].x == hubPos.x, "nothing was duplicated or lost over two round trips");
        g.travelCd = 0;
        check(!g.travelTo(-1, 5, 5, false) && !g.travelTo(B_COUNT, 5, 5, false) && g.world.mapId == B_DESERT, "an invalid destination is refused");
        g.p.dead = true;
        check(!g.travelTo(B_HOME, 40, 30, false) && g.world.mapId == B_DESERT, "a dead hunter cannot travel");
        g.p.dead = false;
        // ---- a gate trigger: standing in a gate's zone travels, once
        g.travelTo(B_HOME, 40, 30, false);
        g.travelCd = 0;
        {
            const Portal& east = MAPS[B_HOME].portals[0];
            g.p.pos = V2((east.zone.x0 + 0.5f) * TS, 84.5f * TS);
            g.checkPortals(0.f);
            check(g.world.mapId == B_DESERT, "stepping into the east gate leads to the desert");
            g.checkPortals(0.f);
            check(g.world.mapId == B_DESERT && g.travelCd > 0, "arriving does not bounce the hunter straight back");
        }
        // ---- saving in another map and loading
        removeAll();
        g.travelCd = 0;
        g.save();
        g.newGame();
        check(g.world.mapId == B_HOME && !g.mapMem[B_DESERT].visited && g.structs.empty(), "a new game starts in the home map with every map reset");
        const bool ld = g.load();
        check(ld && g.world.mapId == B_DESERT && g.curBiome == B_DESERT && spotOk(), "loading a save made in the desert continues in the desert");
        check(g.structs.size() == 1 && g.structs[0].tx == dTx && g.hstate(true, herb).gone && g.world.landmarks[1].found, "the desert's campfire, harvest and discovered landmark are restored");
        g.travelTo(B_HOME, 40, 30, false);
        check(g.structs.size() == 1 && g.structs[0].tx == homeTx && packTotal() == packNow, "the home map's campfire and the pack survive the save / load too");
        removeAll();
        // ---- a save from before the separate maps (one 240x168 world, home area at tile 76,54)
        {
            g.newGame();
            int hn = -1;
            for (size_t i = 0; i < g.world.nodes.size(); i++)
                if (g.world.nodes[i].kind == RN_HERB && (int)(g.world.nodes[i].x / TS) < COX + CW - 5 && (int)(g.world.nodes[i].x / TS) >= COX && (int)(g.world.nodes[i].y / TS) >= COY && (int)(g.world.nodes[i].y / TS) < COY + CH) {
                    hn = (int)i;
                    break;
                }
            const int htx = (int)(g.world.nodes[hn].x / TS), hty = (int)(g.world.nodes[hn].y / TS);
            std::string t = "version 2\ncoins 40\nup 0 0 0 0\nday 30\nkills 0\nquest 0 0 0\ndisc 10000100001000\n";
            t += std::string("slot ") + itemKey(itemWpn(W_FLINT)) + " 8\n";
            t += "hv n " + S(htx) + " " + S(hty) + " " + S(nodeRes(RN_HERB)) + " 120\n";
            t += "hv n 9 9 0 50\n";  // a resource that was out in a biome: ignored
            t += "struct " + S(PL_CHEST) + " " + S(30 + 76) + " " + S(20 + 54) + "\n";                 // a chest in the home area
            t += "struct " + S(PL_CHEST) + " 200 100\n";                                               // a chest out in the biomes
            t += std::string("cslot 0 ") + itemKey(itemMat(M_WOOD)) + " 5\n";
            t += std::string("cslot 1 ") + itemKey(itemMat(M_STONE)) + " 3\n";
            writeFile(t);
            const bool ok = g.load();
            check(ok && g.world.mapId == B_HOME && g.p.coins == 40, "an old save still loads");
            check(g.structs.size() == 1 && g.structs[0].kind == PL_CHEST && g.structs[0].tx == COX + 30 && g.structs[0].ty == COY + 20 && g.structs[0].inv.count(itemMat(M_WOOD)) == 5, "its home chest moves into the home map with its contents");
            check(g.hstate(true, hn).gone, "its harvested herb is still harvested");
            check(g.p.inv.count(itemPlace(PL_CHEST)) == 1 && g.p.mat(M_STONE) == 3, "a chest that stood out in the biomes comes back to the pack with its contents (nothing is lost)");
            check(g.mapMem[B_DESERT].disc == "1000" && g.mapMem[B_SWAMP].disc == "010" && g.mapMem[B_MOUNTAIN].disc == "1000", "old landmark discoveries are split between the maps");
            removeAll();
        }
    }
    printf("skill bonuses\n");
    {
        bool zero = true, mono = true, capped = true, steps = true;
        for (int k = 0; k < SK_COUNT; k++) {
            zero = zero && skillBonusAt(k, 1) == 0.f;
            capped = capped && fabsf(skillBonusAt(k, MAX_LEVEL) - SKILL[k].cap) < 1e-4f && skillBonusAt(k, 500) <= SKILL[k].cap + 1e-4f;
            for (int l = 2; l <= MAX_LEVEL; l++) mono = mono && skillBonusAt(k, l) >= skillBonusAt(k, l - 1);
            for (int m : MILESTONES) steps = steps && skillBonusAt(k, m) - skillBonusAt(k, m - 1) > SKILL[k].cap * 0.09f;  // a milestone is a visible jump
        }
        check(zero, "level 1 gives no bonus");
        check(mono, "bonuses never shrink as you level");
        check(capped, "every bonus tops out exactly at its cap at level 100 and never beyond");
        check(steps, "reaching a milestone level gives a step up in the bonus");
        g.newGame();
        g.state = ST_PLAY;
        g.p.sk.s[SK_DEFENSE].xp = SkillState::capXp();
        g.p.up[U_ARMOR] = 0;
        g.p.hp = g.p.maxhp = 100;
        g.p.invuln = 0;
        g.hurtPlayer(100, V2(0, 0), A_WOLF);
        check((int)g.p.hp == 15, "max Defense cuts incoming damage by its capped 15%");
        g.newGame();
        g.state = ST_PLAY;
        g.p.hp = 10;
        g.p.maxhp = 200;
        g.p.sk.s[SK_SURVIVAL].xp = SkillState::capXp();
        g.p.inv.add(itemCons(C_TONIC), 1);
        g.useConsumable(C_TONIC);
        check((int)g.p.hp == 10 + 117, "max Survival boosts a tonic's healing by its capped 30%");
    }
    printf("skill XP from actions\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.animals.clear();
        Animal* a = g.spawnAnimal(A_RABBIT, g.p.pos + V2(10, 0), 0, 0);
        g.killAnimal(*a);
        check(g.p.sk.s[SK_HUNTING].xp > 0 && g.p.sk.s[SK_RANGED].xp > 0, "a kill trains Hunting and Ranged");
        const int64_t easy = g.p.sk.s[SK_HUNTING].xp;
        Animal* w = g.spawnAnimal(A_WOLF, g.p.pos + V2(30, 0), 0, 0);
        const int64_t b1 = g.p.sk.s[SK_HUNTING].xp;
        g.killAnimal(*w);
        check(g.p.sk.s[SK_HUNTING].xp - b1 > easy, "a wolf is worth more Hunting XP than a rabbit");
        g.p.sk.s[SK_HUNTING].xp = SkillState::capXp() / 2;
        const int hi = g.p.sk.level(SK_HUNTING);
        check(hi > 40 && xpFalloff(hi, 10) < 1.0 && xpFalloff(hi, 10) >= 0.2 && xpFalloff(5, 10) == 1.0, "farming weak animals at a high level pays less (floor 20%)");
        g.p.dead = true;
        const int64_t bd = g.p.sk.s[SK_DEFENSE].xp;
        g.hurtPlayer(10, V2(0, 0), A_WOLF);
        check(g.p.sk.s[SK_DEFENSE].xp == bd, "no Defense XP while dead");
        g.p.dead = false;
        g.p.up[U_ARMOR] = 1;
        g.p.hp = g.p.maxhp = 100;
        g.p.invuln = 0;
        g.hurtPlayer(40, V2(0, 0), A_WOLF);
        check(g.p.sk.s[SK_DEFENSE].xp > bd, "surviving a hit in armor trains Defense");
        g.p.invuln = 0;
        g.p.hp = 5;
        const int64_t bk = g.p.sk.s[SK_DEFENSE].xp;
        g.hurtPlayer(40, V2(0, 0), A_WOLF);
        check(g.p.sk.s[SK_DEFENSE].xp == bk, "a fatal hit earns no Defense XP");
    }
    printf("trophies\n");
    {
        auto owners = [](int m) {  // how many species can drop material m (base drop, form drop or legendary drop)
            int n = 0;
            for (int k = 0; k < A_COUNT; k++)
                if (ANIMAL[k].drop[m][1] > 0 || ANIMAL[k].formDrop[0] == m || ANIMAL[k].legendDrop[0] == m) n++;
            return n;
        };
        bool oneEach = true, exclusive = true;
        for (int k = 0; k < A_COUNT; k++) {
            const int tm = M_TROPHY0 + k;
            if (ANIMAL[k].drop[tm][1] <= 0) {
                oneEach = false;
                printf("    %s has no trophy drop\n", ANIMAL[k].name);
            }
            if (owners(tm) != 1) {
                exclusive = false;
                printf("    %s is dropped by %d species\n", MAT_NAME[tm], owners(tm));
            }
        }
        check(oneEach && M_TROPHY_N == A_COUNT, "every one of the 28 species drops its own trophy");
        check(exclusive, "every trophy is dropped by exactly one species");
        bool noStray = true;  // no species drops a trophy that belongs to another
        for (int k = 0; k < A_COUNT; k++)
            for (int m = M_TROPHY0; m < M_TROPHY0 + M_TROPHY_N; m++)
                if (m != M_TROPHY0 + k && (ANIMAL[k].drop[m][1] > 0 || ANIMAL[k].formDrop[0] == m || ANIMAL[k].legendDrop[0] == m)) noStray = false;
        check(noStray, "no species drops another species' trophy");
        bool tables = true, ranges = true;
        for (int m = 0; m < M_COUNT; m++) {
            tables = tables && MAT_NAME[m] && *MAT_NAME[m] && MAT_BLURB[m] && *MAT_BLURB[m] && MAT_PRICE[m] > 0;
            for (int k = 0; k < A_COUNT; k++) ranges = ranges && ANIMAL[k].drop[m][0] >= 0 && ANIMAL[k].drop[m][0] <= ANIMAL[k].drop[m][1];
        }
        check(tables, "every material has a name, a blurb and a sell price (no table is short)");
        check(ranges, "every drop range is valid (0 <= min <= max)");
        bool permutation = true;
        int seen[M_COUNT] = {0};
        for (int i = 0; i < M_COUNT; i++) {
            if (GATHER_ORDER[i] < 0 || GATHER_ORDER[i] >= M_COUNT || seen[GATHER_ORDER[i]]++) permutation = false;
        }
        check(permutation, "the gather order lists every material exactly once");
        bool keysOk = true;  // save keys are item names: a duplicate would make two items share a key
        for (int a = 0; a < ITEM_COUNT && keysOk; a++)
            for (int b = a + 1; b < ITEM_COUNT; b++)
                if (itemKey(a) == itemKey(b)) {
                    keysOk = false;
                    printf("    duplicate item key %s\n", itemKey(a).c_str());
                    break;
                }
        check(keysOk, "item names are unique, so save keys are unique");
        bool icons = true;
        for (int m = 0; m < M_COUNT; m++) icons = icons && g.A.matIcon[m].t != nullptr;
        check(icons, "every material has an inventory icon");
        bool classes = isCarcassMat(M_MEAT) && isCarcassMat(M_CLAW) && isCarcassMat(M_WOLFFANG) && isCarcassMat(M_FAT) && !isCarcassMat(M_WOOD) && !isCarcassMat(M_GOLDBAR) &&
                       isTrophy(M_COUGARFANG) && !isTrophy(M_FAT) && !isTrophy(M_CLAW);
        check(classes, "carcass / trophy classification is right at every boundary");
        // the real thing: kill every species 60 times and look at what the carcass holds
        g.newGame();
        g.state = ST_PLAY;
        bool ownDrops = true, guaranteed = true, strayKill = true;
        for (int k = 0; k < A_COUNT; k++) {
            const int tm = M_TROPHY0 + k;
            int got = 0;
            for (int i = 0; i < 60; i++) {
                g.animals.clear();
                Animal* a = g.spawnAnimal(k, g.p.pos + V2(40, 0), 0, 0);
                g.killAnimal(*a);
                const Animal& c = g.animals[0];
                got += c.remain[tm];
                if (ANIMAL[k].drop[tm][0] >= 1 && c.remain[tm] < 1) guaranteed = false;
                for (int m = M_TROPHY0; m < M_TROPHY0 + M_TROPHY_N; m++)
                    if (m != tm && c.remain[m] > 0) strayKill = false;
            }
            if (got <= 0) {
                ownDrops = false;
                printf("    %s never dropped %s in 60 kills\n", ANIMAL[k].name, MAT_NAME[tm]);
            }
        }
        check(ownDrops, "killing each species yields its trophy");
        check(guaranteed, "species with a guaranteed trophy always carry it");
        check(strayKill, "a carcass never holds another species' trophy");
        // gathering moves trophies and fat into the pack
        g.newGame();
        g.state = ST_PLAY;
        Animal* w = g.spawnAnimal(A_WOLF, g.p.pos + V2(10, 0), 0, 0);
        g.killAnimal(*w);
        Animal& wc = *w;
        for (int i = 0; i < 100 && wc.totalRemain > 0; i++) g.gatherStep(wc);
        check(wc.totalRemain == 0 && g.p.mat(M_WOLFFANG) >= 1 && g.p.mat(M_FAT) >= 1, "gathering a wolf carcass delivers its fang and fat to the pack");
        // trophies stack, sell and save like any material
        g.p.inv.add(itemMat(M_LIONMANE), 120);
        check(g.p.mat(M_LIONMANE) == 120 && g.p.inv.usedSlots() >= 2, "trophies stack to 99 per slot");
        const int coinsBefore = g.p.coins;
        g.sellMat(M_LIONMANE, 20);
        check(g.p.mat(M_LIONMANE) == 100 && g.p.coins == coinsBefore + 20 * MAT_PRICE[M_LIONMANE], "trophies sell at their listed price");
        g.p.inv.clear();
        g.p.inv.add(itemMat(M_BEARGALL), 3);
        g.p.inv.add(itemMat(M_FAT), 7);
        g.save();
        g.newGame();
        check(g.load() && g.p.mat(M_BEARGALL) == 3 && g.p.mat(M_FAT) == 7, "trophies and fat survive a save / load");
    }
    printf("skills\n");
    {
        // the curve: strictly increasing, nothing past the cap, level derived from total XP
        bool inc = true;
        for (int l = 1; l + 1 < MAX_LEVEL; l++) inc = inc && xpForLevel(l + 1) > xpForLevel(l);
        check(inc && xpForLevel(MAX_LEVEL) == 0 && xpForLevel(1) == (int64_t)XP_BASE, "XP per level rises with level and stops at the cap");
        SkillSet ss;
        check(ss.level(SK_HUNTING) == 1 && ss.add(SK_HUNTING, xpForLevel(1) - 1) == 0 && ss.level(SK_HUNTING) == 1, "one XP short of a level does not level up");
        check(ss.add(SK_HUNTING, 1) == 1 && ss.level(SK_HUNTING) == 2 && ss.s[SK_HUNTING].into() == 0, "exact XP levels up and carries zero over");
        check(ss.add(SK_HUNTING, xpForLevel(2) + xpForLevel(3) + 5) == 2 && ss.level(SK_HUNTING) == 4 && ss.s[SK_HUNTING].into() == 5, "a big award can gain several levels at once");
        check(ss.add(SK_MINING, 0) == 0 && ss.add(SK_MINING, -50) == 0 && ss.add(99, 10) == 0 && ss.add(-1, 10) == 0 && ss.s[SK_MINING].xp == 0, "zero, negative and invalid awards do nothing");
        ss.add(SK_RANGED, (int64_t)1 << 40);
        check(ss.level(SK_RANGED) == MAX_LEVEL && ss.s[SK_RANGED].xp == SkillState::capXp(), "XP is clamped at the level cap");
        bool keys = true;
        for (int a = 0; a < SK_COUNT; a++)
            for (int b = a + 1; b < SK_COUNT; b++) keys = keys && strcmp(SKILL[a].key, SKILL[b].key) != 0 && strcmp(SKILL[a].name, SKILL[b].name) != 0;
        check(keys, "skill keys and names are unique");
        // in the game: level-up reporting, dead players earn nothing
        g.newGame();
        g.state = ST_PLAY;
        check(g.grantXp(SK_WOODCUTTING, 3) == 0 && g.p.sk.s[SK_WOODCUTTING].xp == 3, "grantXp adds XP below a level-up");
        check(g.grantXp(SK_WOODCUTTING, (int)xpForLevel(1)) == 1 && g.p.sk.level(SK_WOODCUTTING) == 2, "grantXp reports a level-up");
        g.p.dead = true;
        check(g.grantXp(SK_MINING, 50) == 0 && g.p.sk.s[SK_MINING].xp == 0, "a dead hunter earns no XP");
        g.p.dead = false;
        // persistence
        g.p.sk.s[SK_COOKING].xp = 12345;
        g.p.sk.s[SK_DEFENSE].xp = 7;
        g.save();
        g.newGame();
        check(g.p.sk.s[SK_COOKING].xp == 0, "(setup) a new game starts with no skill XP");
        check(g.load() && g.p.sk.s[SK_COOKING].xp == 12345 && g.p.sk.s[SK_DEFENSE].xp == 7 && g.p.sk.s[SK_WOODCUTTING].xp > 0, "skill XP survives a save / load");
        check(g.p.sk.s[SK_MINING].xp == 0, "skills that were never trained stay at zero");
        {   // damaged / foreign lines: clamped or ignored, the rest of the save still loads
            std::ifstream in(g.savePath(), std::ios::binary);
            std::string txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();
            std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
            out << txt << "skill futureskill 99\nskill hunting -5\nskill ranged 99999999999999\nskill\nskill mining\n";
        }
        check(g.load() && g.p.sk.s[SK_HUNTING].xp >= 0 && g.p.sk.s[SK_RANGED].xp == SkillState::capXp() && g.p.sk.s[SK_COOKING].xp == 12345, "unknown, negative, oversized and truncated skill lines are handled");
        {   // a save from before skills existed has no skill lines: everything loads at level 1
            std::ifstream in(g.savePath(), std::ios::binary);
            std::string line, kept;
            while (std::getline(in, line))
                if (line.compare(0, 5, "skill") != 0) kept += line + "\n";
            in.close();
            std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
            out << kept;
        }
        bool allOne = g.load();
        for (int k = 0; k < SK_COUNT; k++) allOne = allOne && g.p.sk.level(k) == 1;
        check(allOne, "an older save without skill lines loads with every skill at level 1");
    }
    if (hadSave) {
        std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
        out << savedBytes;
    } else
        removeAll();
    printf("SELFTEST: %s (%d failed)\n", bad ? "FAILED" : "ok", bad);
    return bad;
}

int main(int argc, char** argv) {
    bool headless = false;
    std::string shots, tourDir, dumpFile, wildDir;
    bool wtest = false, stest = false;
    bool wcheck = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--headless")) headless = true;
        if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots = argv[++i];
        if (!strcmp(argv[i], "--tour") && i + 1 < argc) tourDir = argv[++i];
        if (!strcmp(argv[i], "--dumpcore") && i + 1 < argc) dumpFile = argv[++i];
        if (!strcmp(argv[i], "--wildlife") && i + 1 < argc) wildDir = argv[++i];
        if (!strcmp(argv[i], "--weapons")) wtest = true;
        if (!strcmp(argv[i], "--selftest")) stest = true;
        if (!strcmp(argv[i], "--worldcheck")) wcheck = true;
    }
    if (headless) {
        setenv("SDL_VIDEODRIVER", "dummy", 1);
        setenv("SDL_AUDIODRIVER", "dummy", 1);
        setenv("SDL_RENDER_DRIVER", "software", 1);
    }
    Game g;
    if (!g.init(headless)) return 1;
    if (!dumpFile.empty()) {
        dumpCore(g, dumpFile.c_str());
        g.audio.shutdown();
        return 0;
    }
    if (wcheck) {
        int bad = worldCheck(g);
        g.audio.shutdown();
        return bad ? 2 : 0;
    }
    if (stest) {
        int bad = selfTest(g);
        g.audio.shutdown();
        return bad ? 2 : 0;
    }
    if (wtest) {
        int bad = weaponTest(g);
        g.audio.shutdown();
        return bad ? 2 : 0;
    }
    if (!wildDir.empty()) {
        wildlife(g, wildDir);
        g.audio.shutdown();
        return 0;
    }
    if (!tourDir.empty()) {
        tour(g, tourDir);
        g.audio.shutdown();
        return 0;
    }
    if (!shots.empty()) {
        scriptedRun(g, shots);
        g.audio.shutdown();
        return 0;
    }
    Uint64 prev = SDL_GetPerformanceCounter();
    double acc = 0;
    const double step = 1.0 / 60.0;
    while (g.running) {
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - prev) / SDL_GetPerformanceFrequency();
        prev = now;
        if (dt > 0.1) dt = 0.1;
        acc += dt;
        pollInput(g);
        // edge-triggered input must only be consumed once per frame even if several sim steps run
        bool first = true;
        while (acc >= step) {
            g.update((float)step);
            acc -= step;
            if (first) {
                g.in.lPressed = g.in.lReleased = g.in.ePressed = g.in.escPressed = g.in.tabPressed = g.in.enterPressed = false;
                g.in.bandage = g.in.mapToggle = g.in.helpToggle = g.in.invToggle = false;
                g.in.selectKey = -1;
                g.in.wheel = 0;
                first = false;
            }
        }
        g.render();
        g.in.uiClick = false;  // a UI click is only valid for the frame it happened in
        g.present();
    }
    g.shutdown();
    return 0;
}
