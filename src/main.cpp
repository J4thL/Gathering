// main.cpp - entry point, input and fixed-timestep loop.
// Test flags:  --headless (dummy video/audio)  --shots DIR (write screenshots then exit)
//              --forest DIR (close-up screenshots of the home forest's floors)
//              --worldcheck (reachability + walking test)  --tour DIR (screenshots of every map)  --dumpcore FILE (home-area dump)
#include <map>
#include <set>
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <queue>
#include <algorithm>
#include <fstream>
#include "core/game.hpp"
#include "art/export_sprites.hpp"
#include "tools/tool_modes.hpp"
#include <csignal>
#ifdef __GLIBC__
#include <execinfo.h>
#endif

// ---- crash log: if the game dies from a signal (segfault, abort...), wildwood_crash.txt is written next to the save with what the game
// was doing, so a crash always leaves a trace. Best effort: it is written from inside the signal handler.
static Game* crashGame = nullptr;
static char crashPath[1024] = "wildwood_crash.txt";
static void crashHandler(int sig) {
    static volatile int busy = 0;
    if (!busy) {
        busy = 1;
        if (FILE* f = fopen(crashPath, "w")) {
            fprintf(f, "WILDWOOD HUNTER crashed: signal %d\n", sig);
            if (crashGame) {
                const Game& g = *crashGame;
                fprintf(f, "state %d  map %d  delve %d  interior %d  endless floor %d  hires %d  window %dx%d  fullscreen %d\n", (int)g.state, g.world.mapId, g.delve,
                        g.interior, g.endlessFloor, LS > 1 ? 1 : 0, g.winW, g.winH, g.fullscreen ? 1 : 0);
                fprintf(f, "player %.0f %.0f  hp %.0f\n", g.p.pos.x, g.p.pos.y, g.p.hp);
            }
#ifdef __GLIBC__
            fflush(f);
            void* bt[40];
            const int n = backtrace(bt, 40);
            backtrace_symbols_fd(bt, n, fileno(f));
#endif
            fclose(f);
        }
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

void enterMap(Game& g, int b);                          // world_check.cpp (shared with the tours)
V2 portalPoint(const World& w, const Portal& pt);      // world_check.cpp

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
                } else if (e.button.button == SDL_BUTTON_RIGHT) in.rPressed = true;
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
                if (g.rebind >= 0) {  // the settings screen is waiting for a key
                    g.rebindKey(e.key.keysym.scancode);
                    break;
                }
                if (const SDL_Scancode sc = e.key.keysym.scancode; sc != SDL_SCANCODE_UNKNOWN && !keyReserved(sc)) {  // the rebindable keys
                    const auto& K = g.cfg.key;
                    if (sc == K[K_USE]) in.ePressed = true;
                    else if (sc == K[K_HEAL]) in.bandage = true;
                    else if (sc == K[K_MAP]) in.mapToggle = true;
                    else if (sc == K[K_HELP]) in.helpToggle = true;
                    else if (sc == K[K_PACK]) in.invToggle = true;
                    else if (sc == K[K_BUILD]) in.buildToggle = true;
                    else if (sc == K[K_ROTATE]) in.rotPressed = true;
                    else if (sc == K[K_DEMOLISH]) in.delPressed = true;
                    else if (sc == K[K_MOUNT]) in.mountPressed = true;
                    else if (sc == K[K_COMMAND]) in.commandPressed = true;
                    else if (sc == K[K_DODGE]) in.dodgePressed = true;
                    break;
                }
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: in.escPressed = true; break;
                    case SDLK_TAB: in.tabPressed = true; break;
                    case SDLK_RETURN: case SDLK_SPACE: in.enterPressed = true; break;
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
                    case SDLK_F3: g.debugAI = !g.debugAI; break;  // the animal AI overlay
                    case SDLK_F11:
                        g.fullscreen = !g.fullscreen;
                        SDL_SetWindowFullscreen(g.win, g.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                        g.saveSettings();
                        break;
                    default: break;
                }
                break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) in.l = false;
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED && !g.fullscreen && e.window.data1 >= 400 && e.window.data2 >= 225) {
                    g.winW = e.window.data1;  // remembered for next time (written a moment after the last resize)
                    g.winH = e.window.data2;
                    g.winSaveT = 1.0f;
                }
                break;
        }
    }
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    const auto& K = g.cfg.key;
    in.up = k[K[K_UP]] || k[SDL_SCANCODE_UP];
    in.down = k[K[K_DOWN]] || k[SDL_SCANCODE_DOWN];
    in.left = k[K[K_LEFT]] || k[SDL_SCANCODE_LEFT];
    in.right = k[K[K_RIGHT]] || k[SDL_SCANCODE_RIGHT];
    in.sprint = k[K[K_SPRINT]] || (K[K_SPRINT] == SDL_SCANCODE_LSHIFT && k[SDL_SCANCODE_RSHIFT]);
    in.e = k[K[K_USE]];
    in.melee = k[K[K_MELEE]] || (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
}

int delveTest(Game& g);  // delve_tests.cpp
int selfTest(Game& g);       // selftest.cpp
int weaponTest(Game& g);     // weapon_tests.cpp
int worldCheck(Game& g);     // world_check.cpp
int delveCheck(Game& g);     // world_check.cpp
int homesteadTest(Game& g);  // homestead_tests.cpp
int bossTest(Game& g);       // boss_tests.cpp
int soakTest(Game& g);       // soak_tests.cpp
void delveShots(Game& g, const std::string& dir);

#ifdef _WIN32
void attachParentConsole();  // win_console.cpp
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    if (argc > 1) attachParentConsole();  // test and tool runs print into the terminal they were started from
#endif
    bool headless = false;
    std::string printsDir, smithDir, delveDir, forestDir, shots, tourDir, townDir, spriteDir, dumpFile, wildDir, exportDir;
    bool wtest = false, stest = false;
    bool wcheck = false, dtest = false, htest = false, btest = false, soak = false;
    // flags that take a folder / file: without one the old code silently opened the interactive game (and never returned in a script)
    auto takes = [&](int& i, const char* flag, std::string& dst) {
        if (strcmp(argv[i], flag)) return false;
        if (i + 1 >= argc || !strncmp(argv[i + 1], "--", 2)) {
            fprintf(stderr, "wildwood: %s needs a folder or file argument, e.g. %s <path>\n", flag, flag);
            exit(2);
        }
        dst = argv[++i];
        return true;
    };
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--headless")) headless = true;
        else if (takes(i, "--prints", printsDir) || takes(i, "--smith", smithDir) || takes(i, "--shots", shots) || takes(i, "--tour", tourDir) || takes(i, "--town", townDir) || takes(i, "--sprites", spriteDir) ||
                 takes(i, "--export-sprites", exportDir) || takes(i, "--dumpcore", dumpFile) || takes(i, "--wildlife", wildDir) ||
                 takes(i, "--delveshots", delveDir) || takes(i, "--forest", forestDir)) {}
        else if (!strcmp(argv[i], "--weapons")) wtest = true;
        else if (!strcmp(argv[i], "--selftest")) stest = true;
        else if (!strcmp(argv[i], "--worldcheck")) wcheck = true;
        else if (!strcmp(argv[i], "--delve")) dtest = true;
        else if (!strcmp(argv[i], "--homestead")) htest = true;
        else if (!strcmp(argv[i], "--boss")) btest = true;
        else if (!strcmp(argv[i], "--soak")) soak = true;
    }
    // The test suites and tool modes save, load and delete save files: they get their own folder, never the player's real save.
    const bool toolRun = headless || wtest || stest || wcheck || dtest || htest || btest || soak || !shots.empty() || !tourDir.empty() || !townDir.empty() || !smithDir.empty() ||
                         !spriteDir.empty() || !exportDir.empty() || !dumpFile.empty() || !wildDir.empty() || !delveDir.empty() || !forestDir.empty() || !printsDir.empty();
    if (toolRun && !(getenv("WILDWOOD_SAVE_DIR") && *getenv("WILDWOOD_SAVE_DIR"))) {
        std::error_code ec;
        const std::string dir = (std::filesystem::temp_directory_path(ec) / "wildwood_test_save").string();
        std::filesystem::create_directories(dir, ec);
        if (!ec) setenv("WILDWOOD_SAVE_DIR", dir.c_str(), 1);
        else {
            fprintf(stderr, "wildwood: cannot make a separate save folder for this run (%s); stopping so the real save is not touched\n", ec.message().c_str());
            return 2;
        }
    }
    if (headless) {
        setenv("SDL_VIDEODRIVER", "dummy", 1);
        setenv("SDL_AUDIODRIVER", "dummy", 1);
        setenv("SDL_RENDER_DRIVER", "software", 1);
    }
#ifdef _WIN32
    SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "permonitorv2");  // a sharp window on high-DPI screens (not stretched by Windows)
#endif
    Game g;
    crashGame = &g;
    if (!toolRun) {
        const std::string sp = g.savePath();
        const size_t k = sp.find_last_of("/\\");
        const std::string cp = (k == std::string::npos ? std::string() : sp.substr(0, k + 1)) + "wildwood_crash.txt";
        snprintf(crashPath, sizeof crashPath, "%s", cp.c_str());
        for (int sg : {SIGSEGV, SIGABRT, SIGFPE, SIGILL}) signal(sg, crashHandler);
    }
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
    if (!delveDir.empty()) {
        delveShots(g, delveDir);
        g.audio.shutdown();
        return 0;
    }
    if (soak) {
        int bad = soakTest(g);
        g.audio.shutdown();
        return bad ? 2 : 0;
    }
    if (btest) {
        int bad = bossTest(g);
        g.audio.shutdown();
        return bad ? 2 : 0;
    }
    if (htest) {
        int bad = homesteadTest(g);
        g.audio.shutdown();
        return bad ? 2 : 0;
    }
    if (dtest) {
        int bad = delveTest(g);
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
    if (!exportDir.empty()) {  // PNG sprite sheets + JSON animation metadata (export_sprites.hpp)
        const int n = sprexp::exportAll(exportDir);
        if (n < 0) fprintf(stderr, "sprite export failed: could not write some files under %s\n", exportDir.c_str());
        else printf("exported %d files to %s\n", n, exportDir.c_str());
        g.audio.shutdown();
        return n < 0 ? 2 : 0;
    }
    if (!spriteDir.empty()) {
        spriteSheet(g, spriteDir);
        return 0;
    }
    if (!printsDir.empty()) {  // the hunter walks east from the meadow over a bank and into a lake while a deer runs ahead: one frame every 4 ticks, for the footprints gif
        g.newGame();
        g.state = ST_PLAY;
        g.helpT = 0;
        g.dayT = DAY_LEN * 0.1f;
        g.animals.clear();
        auto grassy = [&](int x, int y) { const uint8_t t = g.world.tile[y][x]; return t == T_GRASS || t == T_DARK || t == T_LEAF || t == T_NEEDLES || t == T_MOSS; };
        int bx = -1, by = -1, runX = -1;
        for (int y = 14; y < MH - 14 && runX < 0; y++)  // first choice: a long stretch of bank to walk along, grass behind it
            for (int x = 20; x < MW - 28; x++) {
                int c = 0;
                bool ok = true;
                for (int k = 0; k < 12 && ok; k++) { ok = !g.world.solid[y][x + k]; c += g.world.tile[y][x + k] == T_BANK; }
                for (int k = -6; k < 0 && ok; k++) ok = !g.world.solid[y][x + k] && grassy(x + k, y);
                if (ok && c >= 8) { runX = x; by = y; break; }
            }
        for (int y = 14; y < MH - 14 && bx < 0 && runX < 0; y++)  // otherwise: over the bank and into the lake
            for (int x = 18; x < MW - 18; x++) {
                if (g.world.tile[y][x] != T_LAKE_SHALLOW || g.world.tile[y][x + 1] != T_LAKE_MID || g.world.tile[y][x - 1] != T_BANK) continue;
                bool ok = true;
                for (int k = -8; k <= 0 && ok; k++) ok = !g.world.solid[y][x + k];
                for (int k = -9; k <= -3 && ok; k++) ok = grassy(x + k, y);
                for (int k = 1; k <= 3 && ok; k++) ok = !g.world.solid[y][x + k];
                if (ok) { bx = x; by = y; break; }
            }
        if (runX >= 0) bx = runX + 8;
        if (bx < 0) { printf("prints: no lake bank to walk from\n"); g.audio.shutdown(); return 1; }
        g.p.pos = V2((runX >= 0 ? runX - 5 : bx - 9) * TS + 8.f, by * TS + 8.f);
        g.p.coins = 0;
        g.setCam(0, true);
        if (Animal* d = g.spawnAnimal(A_DEER, V2((bx - 4) * TS + 8.f, by * TS + 8.f), 0, 0)) d->st = AS_FLEE;
        int n = 0;
        for (int f = 0; f < 420; f++) {
            g.in.right = f < 330;  // the last second stands still: the trail fades
            g.update(1.f / 60.f);
            g.in.lPressed = g.in.lReleased = g.in.ePressed = false;
            if (f % 60 == 0) printf("  tick %d: tile under hunter %d, hunter prints %d, all %d\n", f, (int)g.world.tileAt(g.p.pos.x, g.p.pos.y), g.footprintsActive(0), g.footprintsActive());
            if (f % 4 == 0) {
                g.render();
                char nm[64];
                snprintf(nm, sizeof nm, "/fp_%03d.bmp", n++);
                g.saveCanvas((printsDir + nm).c_str());
            }
        }
        g.in.right = false;
        printf("prints: %d frames in %s\n", n, printsDir.c_str());
        g.audio.shutdown();
        return 0;
    }
    if (!forestDir.empty()) {  // one screenshot centred on a patch of each forest floor and of a stream
        g.newGame();
        g.state = ST_PLAY;
        g.helpT = 0;
        g.dayT = DAY_LEN * 0.1f;
        g.animals.clear();
        const char* names[4] = {"leaf", "needles", "moss", "stream"};
        const uint8_t kinds[4] = {T_LEAF, T_NEEDLES, T_MOSS, T_SHALLOW};
        for (int k = 0; k < 4; k++) {
            int bx = -1, by = -1, best = -1000000;
            for (int y = 12; y < MH - 12; y += 3)  // the 25x14-tile window with the most tiles of this floor
                for (int x = 14; x < MW - 14; x += 3) {
                    int c = 0;
                    for (int yy = y - 7; yy <= y + 7; yy += 2)
                        for (int xx = x - 12; xx <= x + 12; xx += 2) c += g.world.tile[yy][xx] == kinds[k];
                    const int score = kinds[k] == T_SHALLOW ? (c < 6 ? -999 : -std::abs(c - 14)) : c;  // a stream: a typical stretch, not a confluence
                    if (score > best && c > 0) { best = score; bx = x; by = y; }
                }
            if (bx < 0) continue;
            g.p.pos = V2(bx * TS + 8.f, by * TS + 8.f);
            g.setCam(0, true);
            g.render();
            g.saveCanvas((forestDir + "/forest_" + names[k] + ".bmp").c_str());
        }
        const char* gnames[GL_COUNT] = {"flowers", "oak", "stone", "fungi", "herbs", "camp"};
        for (int k = 0; k < GL_COUNT; k++)
            for (const Glade& gl : g.world.glades)
                if (gl.kind == k) {
                    g.p.pos = V2(gl.cx * TS + 8.f, gl.cy * TS + 8.f);
                    g.setCam(0, true);
                    g.render();
                    g.saveCanvas((forestDir + "/glade_" + gnames[k] + ".bmp").c_str());
                    break;
                }
        {  // a map of the whole home forest, 4 px per tile: ground colours, trees as dark dots, glades circled (overview.ppm)
            const int Z = 4;
            std::vector<unsigned char> px((size_t)MW * Z * MH * Z * 3);
            auto put = [&](int x, int y, u32 c) {
                if (x < 0 || y < 0 || x >= MW * Z || y >= MH * Z) return;
                unsigned char* o = &px[((size_t)y * MW * Z + x) * 3];
                o[0] = (c >> 16) & 255; o[1] = (c >> 8) & 255; o[2] = c & 255;
            };
            for (int y = 0; y < MH * Z; y++)
                for (int x = 0; x < MW * Z; x++) put(x, y, World::miniTileColor(g.world.tile[y / Z][x / Z]) & 0xFFFFFF);
            for (const Prop& pr : g.world.props)
                if (isTreeKind(pr.kind)) {
                    const int cx = (int)(pr.x / TS * Z), cy = (int)((pr.y - 1.f) / TS * Z);
                    for (int dy = 0; dy < 3; dy++)
                        for (int dx = 0; dx < 3; dx++) put(cx + dx - 1, cy + dy - 1, pr.kind == P_PINE ? 0x174a3a : (pr.kind == P_OAK0 || pr.kind == P_OAK1 ? 0x2a6a1e : 0x6a5a4a));
                }
            for (const Glade& gl : g.world.glades)
                for (int a = 0; a < 360; a += 3) put((int)((gl.cx + cosf(a * 0.01745f) * gl.rx) * Z), (int)((gl.cy + sinf(a * 0.01745f) * gl.ry) * Z), 0xFFE060);
            for (const FlowerField& fl : g.world.fields)
                for (int a = 0; a < 360; a += 2) put((int)((fl.cx + cosf(a * 0.01745f) * fl.rx) * Z), (int)((fl.cy + sinf(a * 0.01745f) * fl.ry) * Z), 0xFF70C0);
            FILE* f = fopen((forestDir + "/overview.ppm").c_str(), "wb");
            if (f) {
                fprintf(f, "P6\n%d %d\n255\n", MW * Z, MH * Z);
                fwrite(px.data(), 1, px.size(), f);
                fclose(f);
            }
        }
        {  // the two fields behind the forest floor, as grey images (density.ppm: trees and thickets; moisture.ppm: wet and dry), for tuning
            const char* fname[2] = {"density", "moisture"};
            for (int k = 0; k < 2; k++) {
                FILE* f = fopen((forestDir + "/" + fname[k] + ".ppm").c_str(), "wb");
                if (!f) continue;
                fprintf(f, "P6\n%d %d\n255\n", MW, MH);
                for (int y = 0; y < MH; y++)
                    for (int x = 0; x < MW; x++) {
                        const float v = k == 0 ? fbm((x - COX) * 0.08f, (y - COY) * 0.08f, g.world.seed + 1) : fbm((x - COX) * 0.045f, (y - COY) * 0.045f, g.world.seed + 31);
                        const unsigned char c = (unsigned char)(255.f * std::min(1.f, std::max(0.f, v)));
                        fputc(c, f); fputc(c, f); fputc(c, f);
                    }
                fclose(f);
            }
        }
        if (!g.world.fields.empty()) {  // one screenshot in the middle of the first flower field
            const FlowerField& f = g.world.fields[0];
            g.p.pos = V2(f.cx * TS + 8.f, f.cy * TS + 8.f);
            g.setCam(0, true);
            g.render();
            g.saveCanvas((forestDir + "/field_flowers.bmp").c_str());
        }
        {  // the AI debug overlay (F3) at dusk, where most animals are within sight of one spot
            g.dayT = DAY_LEN * 0.44f;
            g.populate();
            for (int i = 0; i < 240; i++) g.updateAnimals(0.1f);  // let the routines get going
            int bx = 0, by = 0, best = -1;
            for (auto& a : g.animals) {
                int n = 0;
                for (auto& o : g.animals) n += !o.dead && fabsf(o.pos.x - a.pos.x) < 190 && fabsf(o.pos.y - a.pos.y) < 100;
                if (n > best) { best = n; bx = (int)a.pos.x; by = (int)a.pos.y; }
            }
            g.p.pos = V2(bx + 70.f, by + 20.f);
            g.p.walking = true;
            g.setCam(0, true);
            g.debugAI = true;
            g.render();
            g.saveCanvas((forestDir + "/ai_debug.bmp").c_str());
            g.debugAI = false;
        }
        {  // a wolf pack on its rounds at night, with the AI overlay on (the green lines run to the leader)
            g.dayT = DAY_LEN * 0.7f;
            g.animals.clear();
            g.nextPack = 1;
            g.populate();
            int li = -1;
            for (size_t i = 0; i < g.animals.size(); i++) if (g.animals[i].leader) { li = (int)i; break; }
            if (li >= 0) {
                for (int i = 0; i < 200; i++) g.updateAnimals(0.1f);  // let the pack get under way
                for (size_t i = 0; i < g.animals.size(); i++) if (g.animals[i].leader) { li = (int)i; break; }
                g.p.pos = g.animals[li].pos + V2(150.f, 40.f);
                g.p.walking = true;
                g.setCam(0, true);
                g.debugAI = true;
                g.render();
                g.saveCanvas((forestDir + "/pack_debug.bmp").c_str());
                g.debugAI = false;
            }
        }
        printf("forest floors + glades + fields: screenshots in %s\n", forestDir.c_str());
        g.audio.shutdown();
        return 0;
    }
    if (!smithDir.empty()) {
        smithShots(g, smithDir);
        g.audio.shutdown();
        return 0;
    }
    if (!townDir.empty()) {
        townTour(g, townDir);
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
        if (g.winSaveT > 0.f && (g.winSaveT -= (float)dt) <= 0.f) g.saveSettings();
        // edge-triggered input must only be consumed once per frame even if several sim steps run
        bool first = true;
        while (acc >= step) {
            g.update((float)step);
            acc -= step;
            if (first) {
                g.in.lPressed = g.in.lReleased = g.in.ePressed = g.in.escPressed = g.in.tabPressed = g.in.enterPressed = false;
                g.in.bandage = g.in.mapToggle = g.in.helpToggle = g.in.invToggle = false;
                g.in.buildToggle = g.in.rotPressed = g.in.delPressed = g.in.rPressed = g.in.mountPressed = g.in.commandPressed = g.in.dodgePressed = false;
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
