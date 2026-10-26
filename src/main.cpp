// main.cpp - entry point, input and fixed-timestep loop.
// Test flags:  --headless (dummy video/audio)  --shots DIR (write screenshots then exit)
//              --worldcheck (reachability + walking test)  --tour DIR (biome screenshots)  --dumpcore FILE (home-area dump)
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
    for (int m = 0; m < M_COUNT; m++) g.p.mats[m] = 4 + m;
    frames(30);
    shot("02_village.bmp");
    g.helpT = 0;
    g.p.mats[M_MEAT]=0; g.p.mats[M_HIDE]=0; g.p.mats[M_BONE]=0; g.p.mats[M_ORGAN]=0; g.p.mats[M_FUR]=0; g.p.mats[M_TUSK]=0; g.p.mats[M_ANTLER]=0;

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
        g.p.wpn[W_JAVELIN] = 6;
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
            g.p.mats[M_MEAT] = 0;
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
    g.p.mats[M_BONE] = 10;
    g.p.mats[M_HIDE] = 10;
    g.p.mats[M_TUSK] = 5;
    g.p.mats[M_FUR] = 5;
    g.p.mats[M_ANTLER] = 5;
    bool c1 = g.craft(0), c2 = g.craft(1), c3 = g.craft(2);
    printf("crafted bone/axe/javelin: %d %d %d\n", c1, c2, c3);
    g.p.coins = 1000;
    g.p.mats[M_BONE] = 20;
    g.p.mats[M_HIDE] = 20;
    g.p.mats[M_TUSK] = 20;
    g.p.mats[M_FUR] = 20;
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

int main(int argc, char** argv) {
    bool headless = false;
    std::string shots, tourDir, dumpFile;
    bool wcheck = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--headless")) headless = true;
        if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots = argv[++i];
        if (!strcmp(argv[i], "--tour") && i + 1 < argc) tourDir = argv[++i];
        if (!strcmp(argv[i], "--dumpcore") && i + 1 < argc) dumpFile = argv[++i];
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
