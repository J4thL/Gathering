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

// ---- scripted run used for automated screenshots / smoke testing
static void scriptedRun(Game& g, const std::string& dir) {
    auto frames = [&](int n, float dt = 1.f / 60.f) {
        for (int i = 0; i < n; i++) {
            g.update(dt);
            g.in.lPressed = g.in.lReleased = g.in.ePressed = g.in.escPressed = g.in.enterPressed = g.in.tabPressed = false;
            g.in.selectKey = -1;
            g.in.mapToggle = g.in.helpToggle = g.in.invToggle = false;
            g.in.buildToggle = g.in.rotPressed = g.in.delPressed = g.in.rPressed = g.in.mountPressed = g.in.commandPressed = g.in.dodgePressed = false;
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
        g.p.hot[2] = itemWpn(W_JAVELIN);
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
    g.p.pos = g.world.spawn;
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
    g.shopTab = 2;
    frames(2);
    shot("08_shop_buy.bmp");
    g.state = ST_PLAY;

    // gathering and crafting screens
    {
        g.p.inv.maxWeight = 1e6f;  // this check is about slots / items, not weight
        for (int m = M_WOOD; m <= M_GOLDBAR; m++) g.p.inv.set(itemMat(m), 3 + m % 5);
        g.p.inv.add(itemTool(TL_AXE2), 1);
        g.p.inv.add(itemTool(TL_PICK1), 1);
        g.p.inv.add(itemPlace(PL_CAMPFIRE), 1);
        g.p.inv.add(itemCons(C_SALVE), 2);
        g.p.inv.add(itemTool(TL_HOE), 1);  // a hoe in the pack, the watering can equipped: the pack shot shows both
        g.giveFarmTool(TL_CAN);
        g.p.coins = 250;
        g.state = ST_INVENTORY;
        g.invTab = 0;
        frames(3);
        shot("20_pack.bmp");
        g.invSel = 0;
        for (int i = 0; i < g.p.bagCap(); i++)
            if (g.p.inv.slots[i].n > 2) { g.invSel = i; break; }
        g.amountOpen(Game::AMT_DROP, g.invSel);  // the amount slider over the pack
        frames(3);
        shot("20a_slider.bmp");
        g.amountClose();
        for (int k : {A_RABBIT, A_DEER, A_WOLF, A_BOAR}) g.bestKills[k] = 3 + 9 * k;  // the bestiary tab
        g.invTab = 3;
        frames(3);
        shot("20c_bestiary.bmp");
        g.bestMask[A_WOLF] = 5;  // the species card appears on hover
        g.in.mx = 24 + 30;
        g.in.my = 12 + 38 + 3 * 10 + 4;
        frames(3);
        shot("20c2_bestiary_card.bmp");
        g.invTab = 0;
        g.state = ST_PAUSE;  // pause menu, settings and help pages
        frames(3);
        shot("20e_pause.bmp");
        g.openSettings();
        frames(3);
        shot("20f_settings.bmp");
        g.state = ST_PLAY;
        g.openHelp();
        g.helpPage = 4;
        frames(3);
        shot("20g_help.bmp");
        g.state = ST_PLAY;
        g.gateTarget = B_SWAMP;  // a sealed gate's offering panel
        g.gateOpen[B_DESERT] = true;
        g.state = ST_GATE;
        frames(3);
        shot("20d_gate.bmp");
        g.state = ST_MAP;  // the full map with the fog of war
        g.revealAround(V2(g.p.pos.x + 120.f, g.p.pos.y - 60.f));
        frames(3);
        shot("20b_map.bmp");
        g.state = ST_INVENTORY;
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
    g.dayT = DAY_LEN * 0.55f;
    g.showMap = true;
    frames(40);
    shot("09_night.bmp");
    g.state = ST_PAUSE;
    frames(2);
    shot("10_pause.bmp");
    g.state = ST_PLAY;
    {  // low health: the red edge pulse
        const float keep = g.p.hp;
        g.p.hp = g.p.maxhp * 0.12f;
        frames(2);
        shot("11_lowhp.bmp");
        g.p.hp = keep;
    }
    {  // a villager greeting the hunter, and a shopkeeper's welcome
        const V2 keepPos = g.p.pos;
        g.updateVillagers(0.03f);
        if (!g.villagers.empty()) {
            g.p.pos = g.villagers[0].pos + V2(-20, 4);
            g.villagers[0].chatCd = 0;
            frames(3);
            shot("12_villager.bmp");
        }
        g.p.pos = g.world.questGiver + V2(-52, 14);  // Elder Holt speaking
        g.elderChatCd = 0;
        frames(40);  // (the camera glides to the new spot)
        shot("14_elder.bmp");
        g.p.pos = keepPos;
        if (g.enterBuilding(BLD_FLOWER)) {
            g.p.pos = g.world.npcs[0].pos + V2(0, 30);
            frames(90);
            shot("13_shoptalk.bmp");
            g.p.inv.addIgnoringWeight(itemMat(M_HIDE), 8);  // the shop's SELL rows with the amount slider
            g.shopTab = 0;
            g.state = ST_SHOP;
            g.amountOpenSell(-M_HIDE - 1, 0, g.p.mat(M_HIDE));
            frames(3);
            shot("13b_sellslider.bmp");
            g.amountClose();
            g.addRep(30);  // the ORDERS tab and reputation
            g.p.inv.addIgnoringWeight(g.orders[0].item, g.orders[0].n, g.orders[0].q);
            g.shopTab = 2;
            frames(3);
            shot("13c_orders.bmp");
            g.shopTab = 1;  // the BUY list: hunting gear first, then the farm tools, seeds and hay further down
            g.shopScroll = 0;
            frames(3);
            shot("13d_buy.bmp");
            g.shopScroll = 9;
            frames(3);
            shot("13e_buy_farm.bmp");
            g.shopScroll = 0;
            g.shopTab = 0;
            g.state = ST_PLAY;
            g.exitBuilding();
        }
        g.p.pos = keepPos;
    }
    printf("scripted run complete; animals=%zu\n", g.animals.size());
}


// ---- blacksmith shots (flag: --smith DIR): the blacksmith's house from the front at three times of day, to judge its shadow
static void smithShots(Game& g, const std::string& dir) {
    g.newGame();
    g.state = ST_PLAY;
    g.helpT = 0;
    g.animals.clear();
    const BuildingDef& B = BUILDINGS[BLD_SMITH];
    const float times[3] = {0.10f, 0.25f, 0.40f};  // morning, noon, evening (tod, see Game::tod)
    const char* names[3] = {"morning", "noon", "evening"};
    for (int i = 0; i < 3; i++) {
        g.dayT = (times[i] - 0.05f) * DAY_LEN;
        g.shadowBoost = 2.6f;  // darker than the game, to see the shape of the shadow
        g.p.pos = V2((COX + B.cx + BLD_W * 0.5f) * TS, (COY + B.cy + BLD_H + 9.f) * TS);  // the hunter waits out of view, south of the house
        g.setCam(0, true);
        g.camX = (int)((COX + B.cx + BLD_W * 0.5f) * TS) - LW / 2;  // the camera looks at the house itself
        g.camY = (int)((COY + B.cy + 0.5f) * TS) - LH / 2;
        g.render();
        g.saveCanvas((dir + "/smith_" + names[i] + ".bmp").c_str());
    }
    // a small (1) and a large (2) rock standing clear of trees, the same three times of day (the large ones are in the other maps too)
    for (int rc = 1; rc <= 2; rc++) {
        const Prop* rock = nullptr;
        for (int m : {B_HOME, B_MOUNTAIN, B_SNOW, B_DESERT, B_SWAMP}) {
            if (g.world.mapId != m) g.loadMapWorld(m);
            float best = 1e12f;  // the clear rock of this size nearest to where the hunter starts
            for (const Prop& pr : g.world.props) {
                if (rockClass(pr.kind) != rc) continue;
                bool crowded = false;
                for (const Prop& o : g.world.props)
                    if (&o != &pr && (isTreeKind(o.kind) || o.kind >= P_BLD0) && fabsf(o.x - pr.x) < 7.f * TS && fabsf(o.y - pr.y) < 7.f * TS) { crowded = true; break; }
                if (crowded) continue;
                const float d = (pr.x - g.world.spawn.x) * (pr.x - g.world.spawn.x) + (pr.y - g.world.spawn.y) * (pr.y - g.world.spawn.y);
                if (d < best) best = d, rock = &pr;
            }
            if (rock) break;
        }
        if (!rock) continue;
        for (int i = 0; i < 3; i++) {
            g.dayT = (times[i] - 0.05f) * DAY_LEN;
            g.p.pos = V2(rock->x, rock->y + 9.f * TS);
            g.setCam(0, true);
            g.camX = (int)rock->x - LW / 2;
            g.camY = (int)rock->y - LH / 2 - 8;
            g.render();
            g.saveCanvas((dir + "/rock" + std::to_string(rc) + "_" + names[i] + ".bmp").c_str());
        }
    }
    // every oak variant (0..5) standing clear of other trees on grass, at noon: oakv{N}_noon.bmp (checks the oaks' shadow root)
    {
        if (g.world.mapId != B_HOME) g.loadMapWorld(B_HOME);
        for (int b = 0; b < B_COUNT; b++) g.bw[b] = b == B_HOME ? 1.f : 0.f;
        for (int var = 0; var < 6; var++) {
            const Prop* oak = nullptr;
            float best = 1e12f;
            for (int pass = 0; pass < 2 && !oak; pass++)
                for (const Prop& pr : g.world.props) {
                    if (pr.kind != P_OAK0 && pr.kind != P_OAK1) continue;
                    if (oakVariantFor((int)(pr.x / TS), (int)(pr.y / TS)) != var) continue;
                    bool bad = false;
                    for (const Prop& o : g.world.props)
                        if (&o != &pr && (isTreeKind(o.kind) || o.kind >= P_BLD0) && fabsf(o.x - pr.x) < (pass ? 3.f : 6.f) * TS && fabsf(o.y - pr.y) < (pass ? 3.f : 6.f) * TS) { bad = true; break; }
                    for (int wy = -4; wy <= 6 && !bad; wy++)
                        for (int wx = -6; wx <= 6; wx++)
                            if (g.world.water((int)(pr.x / TS) + wx, (int)(pr.y / TS) + wy)) { bad = true; break; }
                    if (bad) continue;
                    const float d = (pr.x - g.world.spawn.x) * (pr.x - g.world.spawn.x) + (pr.y - g.world.spawn.y) * (pr.y - g.world.spawn.y);
                    if (d < best) best = d, oak = &pr;
                }
            printf("oak variant %d: %s\n", var, oak ? "found" : "none clear");
            if (!oak) continue;
            g.dayT = (times[1] - 0.05f) * DAY_LEN;
            g.shadowBoost = 1.f;
            g.p.pos = V2(oak->x, oak->y + 9.f * TS);
            g.setCam(0, true);
            g.camX = (int)oak->x - LW / 2;
            g.camY = (int)oak->y - LH / 2 - 8;
            g.render();
            g.render();
            g.saveCanvas((dir + "/oakv" + std::to_string(var) + "_noon.bmp").c_str());
        }
    }
    // creatures and the other biomes (the hunter, a deer, a rabbit and a wolf standing on open ground at seven times of day, the last two by
    // moonlight; then the desert's palms and cacti): the shadows of everything that is not a tree, at the strength the game really uses
    g.shadowBoost = 1.f;
    const float tt[7] = {0.07f, 0.13f, 0.25f, 0.38f, 0.46f, 0.64f, 0.75f};
    const char* nm[7] = {"dawn", "morning", "noon", "afternoon", "dusk", "moon_low", "moon_high"};
    {
        if (g.world.mapId != B_HOME) g.loadMapWorld(B_HOME);
        V2 at = g.world.spawn;
        float bestD = 1e12f;
        for (int dy = -40; dy <= 40; dy += 4)
            for (int dx = -40; dx <= 40; dx += 4) {  // the clear patch of ground (no prop within six tiles) nearest to the start
                const V2 c((g.world.spawn.x / TS + dx) * TS, (g.world.spawn.y / TS + dy) * TS);
                bool crowded = false;
                for (const Prop& o : g.world.props)
                    if (fabsf(o.x - c.x) < 6.f * TS && fabsf(o.y - c.y) < 6.f * TS) { crowded = true; break; }
                if (!crowded && dx * dx + dy * dy < bestD) bestD = (float)(dx * dx + dy * dy), at = c;
            }
        g.animals.clear();
        const int kinds[3] = {A_DEER, A_RABBIT, A_WOLF};
        const V2 offs[3] = {V2(46, -14), V2(-40, 12), V2(-12, 30)};
        for (int k = 0; k < 3; k++)
            if (Animal* a = g.spawnAnimal(kinds[k], V2(at.x + offs[k].x, at.y + offs[k].y), 0, 0)) a->vel = V2(0, 0), a->face = k == 1 ? -1 : 1;
        for (int i = 0; i < 7; i++) {
            g.dayT = (tt[i] - 0.05f) * DAY_LEN;
            g.p.pos = at;
            g.setCam(0, true);
            g.render();
            g.render();
            g.saveCanvas((dir + "/creatures_" + nm[i] + ".bmp").c_str());
        }
        g.animals.clear();
        g.loadMapWorld(B_DESERT);
        for (int b = 0; b < B_COUNT; b++) g.bw[b] = b == B_DESERT ? 1.f : 0.f;
        const Prop* best = nullptr;
        int bestN = -1;
        for (const Prop& pr : g.world.props) {  // the spot with the most palms and cacti round it
            if (pr.kind != P_PALM) continue;
            int n = 0;
            for (const Prop& o : g.world.props)
                if ((o.kind == P_PALM || o.kind == P_CACTUS0 || o.kind == P_CACTUS1 || o.kind == P_COLUMN) && fabsf(o.x - pr.x) < 12.f * TS && fabsf(o.y - pr.y) < 8.f * TS) n++;
            if (n > bestN) bestN = n, best = &pr;
        }
        if (best) {
            for (int i : {1, 3}) {
                g.dayT = (tt[i] - 0.05f) * DAY_LEN;
                g.p.pos = V2(best->x, best->y + 6.f * TS);
                g.setCam(0, true);
                g.camX = (int)best->x - LW / 2;
                g.camY = (int)best->y - LH / 2 - 8;
                g.render();
                g.render();
                g.saveCanvas((dir + "/desert_" + nm[i] + ".bmp").c_str());
            }
        }
    }
    // the homestead: a house, a shed, walls, a well, a bench, a lamp and a barrel, in the morning and the afternoon
    {
        g.loadMapWorld(B_HOMESTEAD);
        for (int b = 0; b < B_COUNT; b++) g.bw[b] = b == B_HOMESTEAD ? 1.f : 0.f;
        for (int m : {M_WOOD, M_PLANK, M_STONE, M_BRANCH, M_ROPE, M_FIBER, M_IRONBAR, M_GREASE}) g.p.inv.addIgnoringWeight(itemMat(m), 200);
        g.p.coins = 100000;
        int ox = -1, oy = -1;
        for (int ty = 100; ty < 150 && ox < 0; ty++)
            for (int tx = 70; tx < 200 && ox < 0; tx++) {
                bool ok = true;
                for (int y = 0; y < 12 && ok; y++)
                    for (int x = 0; x < 14 && ok; x++) ok = g.hsCanBuild(HP_FENCE, tx + x, ty + y, 0);
                if (ok) ox = tx, oy = ty;
            }
        if (ox >= 0) {
            int placed = 0;
            placed += g.hsPlace(HP_HOUSE, ox + 1, oy + 1, 0);
            placed += g.hsPlace(HP_SHED, ox + 8, oy + 1, 0);
            placed += g.hsPlace(HP_WELL, ox + 12, oy + 6, 0);
            for (int i = 0; i < 5; i++) placed += g.hsPlace(HP_WALL_WOOD, ox + 1 + i, oy + 8, 0);
            placed += g.hsPlace(HP_BENCH, ox + 7, oy + 6, 0);
            placed += g.hsPlace(HP_LAMP, ox + 9, oy + 9, 0);
            placed += g.hsPlace(HP_BARREL, ox + 11, oy + 9, 0);
            printf("homestead scene: %d pieces placed\n", placed);
            for (int i : {1, 3}) {
                g.dayT = (tt[i] - 0.05f) * DAY_LEN;
                g.p.pos = V2((ox + 9) * TS, (oy + 12) * TS);
                g.setCam(0, true);
                g.camX = (ox + 7) * TS - LW / 2;
                g.camY = (oy + 5) * TS - LH / 2;
                g.render();
                g.render();
                g.saveCanvas((dir + "/homestead_" + nm[i] + ".bmp").c_str());
            }
        }
    }
    printf("smith shots: screenshots in %s\n", dir.c_str());
}

// ---- town tour (flag: --town DIR): screenshots of the central town from a grid of camera positions, for checking the layout
static void townTour(Game& g, const std::string& dir) {
    g.newGame();
    g.state = ST_PLAY;
    g.helpT = 0;
    g.dayT = DAY_LEN * 0.1f;
    g.animals.clear();
    int n = 0;
    for (int gy = 0; gy < 3; gy++)
        for (int gx = 0; gx < 3; gx++) {
            g.p.pos = V2((COX + 31.f + gx * 12.f) * TS, (COY + 22.f + gy * 8.f) * TS);
            g.setCam(0, true);
            g.render();
            g.saveCanvas((dir + "/town_" + std::to_string(gy) + std::to_string(gx) + ".bmp").c_str());
            n++;
        }
    printf("town tour: %d screenshots in %s\n", n, dir.c_str());

    // ---- every building: enter, look around, talk, leave
    int bad = 0;
    auto check = [&](bool ok, const std::string& what) {
        printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
        if (!ok) bad++;
    };
    g.newGame();
    g.state = ST_PLAY;
    g.animals.clear();
    g.p.coins = 777;
    g.p.inv.add(itemMat(M_IRON), 5);
    const int coins0 = g.p.coins, iron0 = g.p.inv.count(itemMat(M_IRON));
    const float w0 = g.p.inv.totalWeight();
    check(MW == 240 && MH == 168 && g.world.pixW() == 240 * TS && g.world.pixH() == 168 * TS, "the home map is still 240x168 tiles");
    for (int k = 0; k < BLD_COUNT; k++) {
        printf(" %s\n", BUILDINGS[k].name);
        const V2 door(bldDoorX(k, COX), bldDoorY(k, COY));
        g.p.pos = door + V2(0.5f, 0.25f);  // an odd spot, so "exact" means something
        const V2 before = g.p.pos;
        g.bldCd = 0;
        check(g.doorNear() == k, "the door prompt is offered at the door");
        check(g.townInteract() && g.interior == k && g.world.isInterior() && g.world.interiorKind() == k, "E enters an interior map of its own");
        check(g.world.pixW() == 240 * TS && dist(g.p.pos, g.world.spawn) < 1.f && g.nearExit(), "the hunter stands at the interior entrance, in reach of the exit");
        check(!g.world.blocked(g.p.pos.x, g.p.pos.y, 6.f), "the entrance is a free spot");
        g.setCam(0, true);
        g.helpT = 0;
        g.travelFade = 0;
        g.render();
        g.saveCanvas((dir + "/int_" + std::to_string(k) + ".bmp").c_str());
        // reachability over free tiles from the entrance
        static bool seen[MH][MW];
        memset(seen, 0, sizeof(seen));
        std::vector<std::pair<int, int>> q;
        q.push_back({(int)(g.p.pos.x / TS), (int)(g.p.pos.y / TS)});
        seen[q[0].second][q[0].first] = true;
        for (size_t i = 0; i < q.size(); i++)
            for (int d = 0; d < 4; d++) {
                const int nx = q[i].first + (d == 0) - (d == 1), ny = q[i].second + (d == 2) - (d == 3);
                if (nx < 0 || ny < 0 || nx >= MW || ny >= MH || seen[ny][nx] || g.world.solidTile(nx, ny)) continue;
                seen[ny][nx] = true;
                q.push_back({nx, ny});
            }
        auto reach = [&](V2 target, float within) {
            for (auto& t : q)
                if (dist(V2((t.first + 0.5f) * TS, (t.second + 0.5f) * TS), target) < within) return true;
            return false;
        };
        check(!g.world.npcs.empty() && reach(g.world.npcs[0].pos, 30.f), "the person is reachable (free floor within talking range)");
        bool stationsOk = true;
        for (auto& st : g.world.stations) stationsOk = stationsOk && reach(st, 24.f);
        check(stationsOk, "every station is reachable");
        int blockedProps = 0;
        for (auto& pr : g.world.props)
            if (!g.world.solid[(int)(pr.y / TS)][(int)(pr.x / TS)]) blockedProps++;
        check(blockedProps == 0, "every piece of furniture blocks movement");
        check(g.world.blocked(0, 0, 4) && g.world.blocked(INT_RX * TS + 8, INT_RY * TS + 8, 4) && g.world.blocked((INT_RX + 12) * TS, (INT_RY + 1) * TS + 8, 4), "walls and the void are solid");
        // talk to the person
        {
            V2 stand = g.p.pos;
            float bd = 1e9f;
            for (auto& t : q) {
                const V2 c((t.first + 0.5f) * TS, (t.second + 0.5f) * TS);
                const float d = dist(c, g.world.npcs[0].pos);
                if (d < bd) bd = d, stand = c;
            }
            g.p.pos = stand;
            g.bldCd = 0;
            check(g.npcNear() >= 0, "the person can be talked to");
            g.townInteract();
            const bool opened = k == BLD_SHOP ? g.state == ST_SHOP : g.state == ST_SERVICE && g.svc == k;
            check(opened, "E opens that person's service screen");
            g.travelFade = 0;
            g.render();
            g.saveCanvas((dir + "/svc_" + std::to_string(k) + ".bmp").c_str());
            if (g.state == ST_SERVICE) g.closeService();
            g.state = ST_PLAY;
        }
        g.p.pos = g.world.exitPos;
        g.bldCd = 0;
        check(g.nearExit(), "the exit prompt is offered on the doormat");
        check(g.townInteract() && g.interior == -1 && g.world.mapId == B_HOME, "E on the doormat leaves, back to the home map");
        check(g.p.pos.x == before.x && g.p.pos.y == before.y, "the hunter is back on the exact spot they entered from");
        check(g.p.coins == coins0 && g.p.inv.count(itemMat(M_IRON)) == iron0 && fabsf(g.p.inv.totalWeight() - w0) < 1e-3f, "coins and pack are untouched");
    }
    printf("town interiors: %s (%d failed)\n", bad ? "FAILED" : "all passed", bad);

    // ---- the services
    printf("town services\n");
    int bad2 = 0;
    auto ck = [&](bool ok, const std::string& what) {
        printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
        if (!ok) bad2++;
    };
    auto find = [&](int outItem, int mat0) {
        for (int r = 0; r < RECIPE_N; r++)
            if (recipeItem(RECIPES[r]) == outItem && (mat0 < 0 || RECIPES[r].mat[0] == mat0)) return r;
        return -1;
    };
    g.newGame();
    g.state = ST_PLAY;
    g.animals.clear();
    g.p.coins = 500;
    // crafting is done only in the matching building
    const int rIronBar = find(itemMat(M_IRONBAR), M_IRON), rLeather = find(itemMat(M_LEATHER), M_HIDE), rBouquet = find(itemMat(M_BOUQUET), M_ROSE);
    ck(rIronBar >= 0 && rLeather >= 0 && rBouquet >= 0, "recipes for ingots, leather and bouquets exist");
    g.p.inv.add(itemMat(M_IRON), 6);
    g.p.inv.add(itemMat(M_WOOD), 8);
    g.p.inv.add(itemMat(M_HIDE), 6);
    g.p.inv.add(itemMat(M_GREASE), 2);
    g.interior = -1;
    ck(!g.craft(rIronBar) && g.p.mat(M_IRON) == 6, "ore is not smelted outdoors");
    g.interior = BLD_TANNER;
    ck(!g.craft(rIronBar) && g.p.mat(M_IRON) == 6, "ore is not smelted at the leatherworks either");
    g.interior = BLD_SMITH;
    const int xp0 = g.p.sk.level(SK_SMITHING), w1 = (int)g.p.inv.totalWeight();
    ck(g.craft(rIronBar) && g.p.mat(M_IRONBAR) >= 1 && g.p.mat(M_IRON) == 4, "the blacksmith smelts ore into an ingot (same recipe, same pack)");
    (void)xp0; (void)w1;
    g.interior = BLD_SHOP;
    ck(!g.craft(rLeather), "the general store crafts nothing");
    g.interior = BLD_TANNER;
    ck(g.craft(rLeather) && g.p.mat(M_LEATHER) >= 2 && g.p.mat(M_HIDE) == 3, "the leatherworks tans hide into leather");
    g.interior = -1;
    g.p.coins = 500;
    // flowers
    g.p.inv.clear();
    g.p.coins = 200;
    ck(g.buyFlower(M_ROSE, 1) && g.buyFlower(M_TULIP, 1) && g.buyFlower(M_LAVENDER, 1) && g.p.mat(M_ROSE) == 1 && g.p.coins == 200 - 8 - 6 - 10, "Mary Rose sells flowers for coins into the real pack");
    ck(!g.buyFlower(M_ROSE, 1000) && g.p.coins == 200 - 24, "a purchase that cannot be paid or carried changes nothing");
    g.interior = BLD_WORKSHOP;
    ck(g.craft(rBouquet) && g.p.mat(M_BOUQUET) == 1 && g.p.mat(M_ROSE) == 0, "the workshop ties them into a bouquet");
    g.interior = BLD_FLOWER;
    const int c0 = g.p.coins;
    g.sellMat(M_BOUQUET, 1);
    ck(g.p.coins == c0 + MAT_PRICE[M_BOUQUET] && g.p.mat(M_BOUQUET) == 0, "the bouquet sells for its price");
    g.interior = -1;
    // salon
    g.p.coins = 100;
    g.p.inv.add(itemMat(M_BONE), 3);
    const int up1 = g.p.armorLv(), lv1 = g.p.sk.level(SK_CRAFTING), bone1 = g.p.mat(M_BONE), q1 = g.questIdx;
    const HunterLook def;
    ck(g.p.look == def, "the hunter starts with the default look");
    g.salonLook = def;
    ck(!g.confirmSalon() && g.p.coins == 100, "confirming with no change costs nothing");
    // every option changes the drawn sprite (all three directions)
    bool allDiffer = true;
    for (int opt = 0; opt < 4; opt++) {
        HunterLook l = def;
        const int n = opt == 0 ? LOOK_HAIR_N : opt == 1 ? LOOK_COLOR_N : opt == 2 ? LOOK_SKIN_N : LOOK_OUTFIT_N;
        for (int v = 1; v < n; v++) {
            (opt == 0 ? l.hair : opt == 1 ? l.color : opt == 2 ? l.skin : l.outfit) = v;
            if (opt == 0 && v == 0) continue;
            bool diff = false;
            for (int d = 0; d < 3 && !diff; d++)
                for (int f = 0; f < 4 && !diff; f++) diff = mkPlayer(d, f, l).p != mkPlayer(d, f, def).p;
            allDiffer = allDiffer && diff;
        }
    }
    ck(allDiffer, "every hairstyle, hair colour, skin tone and tunic colour changes the sprite in every direction");
    HunterLook want;
    want.hair = 2; want.color = 6; want.skin = 3; want.outfit = 2;
    g.salonLook = want;
    g.p.coins = 10;
    ck(!g.confirmSalon() && g.p.look == def && g.p.coins == 10, "too few coins: refused, nothing changes");
    g.p.coins = 100;
    ck(g.confirmSalon() && g.p.look == want && g.p.coins == 100 - SALON_FEE && g.A.look == want, "confirm charges the small fee and applies the look to the real sprites");
    ck(g.p.armorLv() == up1 && g.p.sk.level(SK_CRAFTING) == lv1 && g.p.mat(M_BONE) == bone1 && g.questIdx == q1, "inventory, gear, skills and quests are untouched");
    g.salonLook = def;  // a preview that is cancelled / abandoned changes nothing
    g.salonLook = g.p.look;
    ck(g.p.look == want, "cancelling keeps the confirmed look");
    g.save();
    g.newGame();
    ck(g.p.look == def && g.A.look == def, "a new game starts with the default look");
    ck(g.load() && g.p.look == want && g.A.look == want, "the look survives save and load");
    printf("town services: %s (%d failed)\n", bad2 ? "FAILED" : "all passed", bad2);
}

// ---- sprite sheet (flag: --sprites DIR): the hunter (every direction and frame) and every animal's first form, for judging the art
static void spriteSheet(Game& g, const std::string& dir) {
    auto page = [&](const char* name, const std::vector<const Spr*>& list, int cols, int cellW, int cellH) {
        g.targetCanvas();
        SDL_SetRenderDrawColor(g.ren, 96, 168, 80, 255);
        SDL_RenderClear(g.ren);
        for (size_t i = 0; i < list.size(); i++) {
            const Spr& s = *list[i];
            if (!s.ok()) continue;
            SDL_Rect d{(int)(i % cols) * cellW + cellW / 2 - s.ax, (int)(i / cols) * cellH + cellH - 4 - s.ay, s.w, s.h};
            SDL_RenderCopy(g.ren, s.t, nullptr, &d);
        }
        g.saveCanvas((dir + "/" + name + ".bmp").c_str());
    };
    // one page per hunter state: a row per facing (S SE E NE N), a column per pose
    for (int st = 0; st < HS_COUNT; st++) {
        std::vector<const Spr*> hero;
        for (int v = 0; v < FACING_VIEWS; v++)
            for (int i = 0; i < HS_MAXF; i++) hero.push_back(i < hunterPoseCount(st) ? &g.A.hunter[v][st][i] : &g.A.hunter[v][st][0]);
        static const char* NAMES[HS_COUNT] = {"idle", "walk", "run", "aim", "throw", "gather", "swing", "use", "hurt", "death", "mine", "forage"};
        page((std::string("hunter_") + NAMES[st]).c_str(), hero, HS_MAXF, 28, 36);
    }
    for (int pg = 0; pg * 12 < (int)A_COUNT; pg++) {
        std::vector<const Spr*> list;
        for (int k = pg * 12; k < std::min((int)A_COUNT, pg * 12 + 12); k++)
            for (int f : {0, 1, 4}) list.push_back(&g.A.animal[k][0][f]);
        page(("animals" + std::to_string(pg)).c_str(), list, 12, 33, 50);
    }
    // the animals with a full animation set: one page per species and state, a row per facing (S SE E NE N), a column per pose
    for (int kind : RICH_KINDS) {
        if (!g.A.rich[kind]) continue;
        static const char* BN[AN_COUNT] = {"idle", "walk", "run", "attack", "hurt", "death", "fly", "takeoff", "land", "eat", "rest", "alert"};
        for (int st = 0; st < AN_COUNT; st++) {
            std::vector<const Spr*> list;
            const int n = animalClips(kind)->poses[st];
            if (n <= 0) continue;  // a state this species does not have (the flight states belong to birds)
            for (int v = 0; v < AV_VIEWS; v++)
                for (int i = 0; i < AN_MAXF; i++) list.push_back(&g.A.rich[kind]->s[0][v][st][std::min(i, n - 1)]);
            page((std::string(ANIMAL[kind].name) + "_" + BN[st]).c_str(), list, AN_MAXF, 56, 52);
        }
    }
    // the hunter standing in the world in every facing / a few states (shadow, depth, view mirroring)
    g.newGame();
    g.state = ST_PLAY;
    g.animals.clear();
    g.in.mx = 395;
    g.in.my = 220;  // the aim cursor out of the way
    static const int SC_STATE[] = {HS_IDLE, HS_WALK, HS_RUN, HS_SWING, HS_DEATH};
    for (int sti = 0; sti < 5; sti++)
        for (int f = 0; f < F_COUNT; f++) {
            g.p.face8 = f;
            const int st = SC_STATE[sti];
            g.p.anim = Animator();
            g.p.anim.play(st, &hunterClip(st));
            g.p.anim.t = st == HS_RUN ? 0.1f : st == HS_WALK ? 0.15f : st == HS_SWING ? 0.5f : st == HS_DEATH ? 0.3f : 0.f;
            g.p.dead = st == HS_DEATH;
            g.p.deadT = 0.4f;
            g.setCam(0, true);
            g.render();
            g.saveCanvas((dir + "/scene_" + std::to_string(sti) + std::to_string(f) + ".bmp").c_str());
        }
    g.p.dead = false;
    printf("sprite sheets written to %s\n", dir.c_str());
}

// ---- world verification (flags: --worldcheck, --dumpcore FILE, --tour DIR)
// Dump of the original home area's interior (tiles, solid flags, props, decor). Written by the old generator
// and the new one in the same format so the two can be diffed: the start area must be unchanged.
static void dumpCore(Game& g, const char* file) {
    const World& w = g.world;
    const int X0 = 16, X1 = 74, Y0 = 4, Y1 = 56;  // interior of the original area
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

static void tour(Game& g, const std::string& dir) {
    auto frames = [&](int n) {
        for (int i = 0; i < n; i++) {
            g.update(1.f / 60.f);
            g.in.lPressed = g.in.lReleased = g.in.ePressed = g.in.escPressed = g.in.enterPressed = false;
        }
        g.render();
    };
    auto shot = [&](const std::string& name) { g.saveCanvas((dir + "/" + name + ".bmp").c_str()); };
    static const char* nm[B_COUNT] = {"home", "desert", "swamp", "snow", "mountain", "homestead"};
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
            g.dayT = DAY_LEN * 0.2f;  // midday
            g.travelCd = 1e9f;  // a tour must not walk through a gate by accident
            g.setCam(0, true);
            frames(25);
            shot(sp.first);
            if (k == 1) {  // also a night view of each hub, for lanterns / fog / glows
                g.dayT = DAY_LEN * 0.6f;
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
            g.dayT = DAY_LEN * 0.2f;
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
    g.dayT = DAY_LEN * 0.2f;
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
                g.dayT = night ? DAY_LEN * 0.6f : DAY_LEN * 0.2f;
                Animal* a = g.spawnAnimal(k, g.p.pos + V2(60, 0), 0, 0);
                for (int f = 0; f < 90; f++) g.update(1.f / 60.f), g.p.hp = g.p.maxhp, g.dayT = night ? DAY_LEN * 0.6f : DAY_LEN * 0.2f;
                printf("probe %-14s %-5s player 60px away for 1.5s -> %s\n", animalName(k, 0).c_str(), night ? "night" : "day", stn[g.animals[0].st]);
                (void)a;
            }
        }
        g.dayT = DAY_LEN * 0.2f;
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
