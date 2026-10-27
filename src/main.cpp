// main.cpp - entry point, input and fixed-timestep loop.
// Test flags:  --headless (dummy video/audio)  --shots DIR (write screenshots then exit)
//              --worldcheck (reachability + walking test)  --tour DIR (biome screenshots)  --dumpcore FILE (home-area dump)
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
                    case SDLK_1: in.selectKey = 0; break;
                    case SDLK_2: in.selectKey = 1; break;
                    case SDLK_3: in.selectKey = 2; break;
                    case SDLK_4: in.selectKey = 3; break;
                    case SDLK_5: in.selectKey = 4; break;
                    case SDLK_6: in.selectKey = 5; break;
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
            g.in.mapToggle = g.in.helpToggle = false;
        }
        g.render();
    };
    auto shot = [&](const char* name) { g.saveCanvas((dir + "/" + name).c_str()); };

    frames(120);
    shot("01_title.bmp");

    g.newGame();
    g.state = ST_PLAY;
    g.p.coins = 250;
    for (int m = 0; m < M_COUNT; m++) g.p.inv.set(itemMat(m), 4 + m);
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

static int worldCheck(Game& g) {
    World& w = g.world;
    int bad = 0;
    Reach rc(w, w.spawn);
    printf("world %dx%d tiles (%dx%d px); home area at tile (%d,%d)\n", MW, MH, (int)w.pixW(), (int)w.pixH(), COX, COY);
    printf("props=%zu decor=%zu nodes=%zu landmarks=%zu\n", w.props.size(), w.decor.size(), w.nodes.size(), w.landmarks.size());
    // biome area and how much of it the player can actually reach
    long total[B_COUNT] = {0}, reach[B_COUNT] = {0};
    for (int cy = 0; cy < rc.gh; cy++)
        for (int cx = 0; cx < rc.gw; cx++) {
            int b = w.biomeAtPos(cx * 8.f, cy * 8.f);
            if (!w.blocked(cx * 8.f + 4, cy * 8.f + 4, 4.f)) {
                total[b]++;
                if (rc.d[(size_t)cy * rc.gw + cx] >= 0) reach[b]++;
            }
        }
    for (int b = 0; b < B_COUNT; b++)
        printf("  %-10s walkable cells %6ld  reachable from spawn %6ld (%.1f%%)\n", BIOME[b].name, total[b], reach[b], total[b] ? 100.0 * reach[b] / total[b] : 0.0);
    // hubs and landmarks
    for (int b = 1; b < B_COUNT; b++) {
        int d = rc.at(w.hub[b].x, w.hub[b].y);
        printf("  hub %-10s reachable: %s  path length %d px\n", BIOME[b].name, d >= 0 ? "yes" : "NO", d >= 0 ? d * Reach::CELL : -1);
        if (d < 0) bad++;
    }
    for (auto& l : w.landmarks) {
        bool ok = rc.nearReach(l.x, l.y, 2);
        printf("  landmark %-18s (%s) reachable: %s\n", l.name, BIOME[l.biome].name, ok ? "yes" : "NO");
        if (!ok) bad++;
    }
    int nodeBad = 0, nodeCnt[RN_COUNT] = {0};
    for (auto& n : w.nodes) {
        nodeCnt[n.kind]++;
        if (!rc.nearReach(n.x, n.y, 2)) nodeBad++;
    }
    printf("  resource nodes:");
    for (int k = 0; k < RN_COUNT; k++) printf(" %s=%d", NODE_NAME[k], nodeCnt[k]);
    printf("\n  nodes unreachable: %d of %zu\n", nodeBad, w.nodes.size());
    if (nodeBad) bad++;
    // the edge wall must be solid everywhere
    int hole = 0;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
            if ((x < 2 || y < 2 || x >= MW - 2 || y >= MH - 2) && !w.solidTile(x, y)) hole++;
    printf("  edge wall holes: %d\n", hole);
    if (hole) bad++;
    // start area: spawn, shop door, elder and campfire must all be walkable and connected
    printf("  spawn reachable: %s\n", rc.at(w.spawn.x, w.spawn.y) >= 0 ? "yes" : "NO");

    // walk test: drive the real player through update() along the BFS route to each hub
    for (int b = 1; b < B_COUNT; b++) {
        g.newGame();
        g.state = ST_PLAY;
        g.helpT = 0;
        g.p.pos = w.spawn;
        g.setCam(0, true);
        // route: follow strictly decreasing distance-from-hub (BFS from the hub gives the way back)
        Reach back(w, w.hub[b], 6.f);  // planned with a fatter body than the real one (4px) so steering can't clip corners
        int frames = 0;
        float lastD = 1e9f;
        bool arrived = false;
        for (; frames < 60 * 240; frames++) {
            int cx = (int)(g.p.pos.x / 8), cy = (int)(g.p.pos.y / 8);
            int cur = back.at(g.p.pos.x, g.p.pos.y);
            if (cur >= 0 && cur <= 2) {
                arrived = true;
                break;
            }
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
            lastD = (float)cur;
        }
        if (!arrived) printf("    stuck at px (%.1f, %.1f) tile (%d,%d) dist-to-hub cells %d tile=%d solid=%d\n", g.p.pos.x, g.p.pos.y, (int)(g.p.pos.x / TS), (int)(g.p.pos.y / TS), back.at(g.p.pos.x, g.p.pos.y), (int)w.tileAt(g.p.pos.x, g.p.pos.y), (int)w.blocked(g.p.pos.x, g.p.pos.y, 4.f));
        g.in.left = g.in.right = g.in.up = g.in.down = false;
        printf("  walk to %-10s hub: %s after %.0fs (biome now %s, discovered %d)\n", BIOME[b].name, arrived ? "arrived" : "FAILED", frames / 60.f,
               BIOME[g.curBiome].name, g.landmarksFound());
        (void)lastD;
        if (!arrived) bad++;
    }

    // terrain + climate mechanics, measured through the real player update
    {
        auto findPatch = [&](uint8_t t, int biomeWanted, V2& out, int rad = 4) {
            for (int y = 8; y < MH - 8; y++)
                for (int x = 8; x < MW - 8; x++) {
                    if (biomeWanted >= 0 && w.biome[y][x] != biomeWanted) continue;
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
            V2 at;
            if (!findPatch(rw.tile, rw.biome, at, rw.tile == T_MUD ? 2 : 4)) {
                printf("  terrain %-12s no 9x9 patch found (skipped)\n", rw.name);
                continue;
            }
            g.newGame();
            g.state = ST_PLAY;
            g.animals.clear();
            g.respawnT = 1e9f;
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
                V2 at;
                if (!findPatch(c.tile, c.biome, at, c.tile == T_MUD ? 2 : 4)) continue;
                g.newGame();
                g.state = ST_PLAY;
                g.animals.clear();
                g.respawnT = 1e9f;
                g.dayT = 60;  // midday
                g.p.up[U_ARMOR] = armor;
                g.p.pos = at;
                g.curBiome = c.biome;
                g.in.right = true;
                g.in.sprint = true;
                float s0 = g.p.stamina;
                for (int f = 0; f < 60; f++) {
                    g.p.hp = g.p.maxhp;
                    g.dayT = 60;
                    g.update(1.f / 60.f);
                    g.curBiome = c.biome;
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
    g.newGame();
    g.state = ST_PLAY;
    g.helpT = 0;
    g.showMap = true;
    g.animals.clear();
    static const char* nm[B_COUNT] = {"home", "desert", "swamp", "snow", "mountain"};
    for (int b = 0; b < B_COUNT; b++) {
        // hub, then every landmark of that biome
        std::vector<std::pair<std::string, V2>> spots;
        spots.push_back({std::string(nm[b]) + "_hub", b == B_HOME ? g.world.spawn : g.world.hub[b]});
        int i = 0;
        for (auto& l : g.world.landmarks)
            if (l.biome == b) spots.push_back({std::string(nm[b]) + "_lm" + std::to_string(i++), V2(l.x, l.y + 24)});
        int k = 0;
        for (auto& sp : spots) {
            g.p.pos = sp.second;
            g.dayT = 60;  // midday
            g.setCam(0, true);
            for (int x = 0; x < B_COUNT; x++) g.bw[x] = (x == b) ? 1.f : 0.f;
            g.curBiome = b;
            frames(25);
            shot(sp.first);
            if (k == 0) {  // also a night view of each hub, for lanterns / fog / glows
                g.dayT = 300.f * 0.6f;
                frames(20);
                shot(sp.first + "_night");
            }
            k++;
        }
    }
    // frame cost (software renderer, so only the relative numbers mean anything)
    for (int b = 0; b < B_COUNT; b++) {
        g.p.pos = b == B_HOME ? g.world.spawn : g.world.hub[b];
        g.setCam(0, true);
        for (int x = 0; x < B_COUNT; x++) g.bw[x] = (x == b) ? 1.f : 0.f;
        g.curBiome = b;
        Uint64 t0 = SDL_GetPerformanceCounter();
        for (int i = 0; i < 120; i++) {
            g.update(1.f / 60.f);
            g.render();
        }
        double ms = (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency() / 120.0;
        printf("  frame cost at %-8s hub: %.2f ms (update+render)\n", nm[b], ms);
    }
    // the border crossings
    struct X { const char* n; V2 p; } cross[4] = {
        {"gate_east", V2((COX + CW) * TS - 10.f, g.world.village.y)}, {"gate_south", V2(g.world.village.x, (COY + CH) * TS - 10.f)},
        {"gate_north", V2(g.world.village.x, COY * TS + 20.f)}, {"gate_west", V2(COX * TS + 20.f, g.world.village.y)}};
    for (auto& c : cross) {
        g.p.pos = c.p;
        g.dayT = 60;
        g.setCam(0, true);
        frames(25);
        shot(c.n);
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
        g.animals.clear();
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
        g.animals.clear();
        memset(g.spawnStat, 0, sizeof g.spawnStat);
        g.p.pos = g.world.hub[b];
        g.p.maxhp = g.p.hp = 1e6f;
        for (int x = 0; x < B_COUNT; x++) g.bw[x] = (x == b) ? 1.f : 0.f;
        g.curBiome = b;
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
            for (int m = M_FEATHER; m < M_COUNT; m++) orig += ANIMAL[k].drop[m][1];
        check(orig == 0, "the original four animals drop no new materials");
    }
    // --- save round trip
    {
        g.p.coins = 123;
        for (int m = 0; m < M_COUNT; m++) g.p.inv.set(itemMat(m), m + 1);
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
        for (int m = 0; m < M_COUNT; m++) same = same && g.p.mat(m) == keep.mat(m);
        for (int w = 0; w < W_COUNT; w++) same = same && g.p.wpn(w) == keep.wpn(w);
        check(same, "save / load round trip keeps coins, all 11 materials, all 6 weapons and both ammo types");
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
        for (int m = 0; m < M_COUNT; m++) gr.add(itemMat(m), 5);
        check(gr.setCapacity(30) && gr.capacity() == 30 && gr.usedSlots() == M_COUNT, "growing the bag keeps every item");
        check(!gr.setCapacity(5) && gr.capacity() == 30 && gr.usedSlots() == M_COUNT, "shrinking below what is stored is refused (nothing is lost)");
        check(gr.setCapacity(M_COUNT) && gr.capacity() == M_COUNT, "shrinking to exactly what fits works");
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
        check(PACK_SLOTS[0] == 20 && PACK_SLOTS[1] == 30 && PACK_SLOTS[2] == 40, "backpack levels give 20 / 30 / 40 slots");
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
        for (int m = M_TUSK; m < M_COUNT && slotsLeft > 0; m++, slotsLeft--) g.p.inv.add(itemMat(m), 50);
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
        g.gatherStep(c);
        check(g.p.mat(M_MEAT) == before && c.remain[M_MEAT] == remBefore, "gathering is refused when the bag is full and no stack can take it");
        g.p.inv.remove(itemMat(M_TUSK), 50);  // frees one slot
        g.gatherStep(c);
        check(g.p.mat(M_MEAT) >= before + 1, "after freeing a slot, gathering works again");
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
    {   // crafting in a full bag
        g.p.inv.clear();
        fillWithJunk();
        g.p.inv.remove(itemMat(M_TUSK), 50);
        g.p.inv.remove(itemMat(M_ANTLER), 50);
        g.p.inv.remove(itemWpn(W_BONE), 1);  // three free slots: bone, hide and a bit of meat fill them, so the bag is full again
        g.p.inv.add(BONE, 3);
        g.p.inv.add(HIDE, 1);
        g.p.inv.add(MEAT, 1);
        int bonesBefore = g.p.mat(M_BONE);
        bool ok = g.craft(0);  // BONE KNIFE x4: needs 3 bone + 1 hide, the two ingredient stacks vanish, the knives take a slot
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
        bool ok2 = g.craft(0);
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
                g.in.bandage = g.in.mapToggle = g.in.helpToggle = false;
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
