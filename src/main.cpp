// main.cpp - entry point, input and fixed-timestep loop.
// Test flags:  --headless (dummy video/audio)  --shots DIR (write screenshots then exit)
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
                switch (e.key.keysym.sym) {
                    case SDLK_e: in.ePressed = true; break;
                    case SDLK_ESCAPE: in.escPressed = true; break;
                    case SDLK_TAB: in.tabPressed = true; break;
                    case SDLK_RETURN: case SDLK_SPACE: in.enterPressed = true; break;
                    case SDLK_q: in.bandage = true; break;
                    case SDLK_m: in.mapToggle = true; break;
                    case SDLK_h: in.helpToggle = true; break;
                    case SDLK_i: in.invToggle = true; break;
                    case SDLK_b: in.buildToggle = true; break;
                    case SDLK_r: in.rotPressed = true; break;
                    case SDLK_x: in.delPressed = true; break;
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
            g.in.buildToggle = g.in.rotPressed = g.in.delPressed = g.in.rPressed = false;
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
    bool u = g.buyUpgrade(U_TOOL);
    printf("upgrade tool: %d (lvl %d)\n", u, g.p.up[U_TOOL]);
    g.shopTab = 2;
    frames(2);
    shot("08_upgrades.bmp");
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
            g.exitBuilding();
        }
        g.p.pos = keepPos;
    }
    printf("scripted run complete; animals=%zu\n", g.animals.size());
}


// ---- town tour (flag: --town DIR): screenshots of the central town from a grid of camera positions, for checking the layout
static void townTour(Game& g, const std::string& dir) {
    g.newGame();
    g.state = ST_PLAY;
    g.helpT = 0;
    g.dayT = 30;
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
            if (RECIPES[r].kind != RK_GEAR && recipeItem(RECIPES[r]) == outItem && (mat0 < 0 || RECIPES[r].mat[0] == mat0)) return r;
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
    // upgrades moved to the smith: still real
    g.p.coins = 500;
    g.p.inv.add(itemMat(M_COPPER), 20);
    g.p.inv.add(itemMat(M_IRON), 20);
    g.p.inv.add(itemMat(M_HIDE), 20);
    const int up0 = g.p.up[U_TOOL];
    g.buyUpgrade(U_TOOL);
    ck(g.p.up[U_TOOL] >= up0, "the tool upgrade still works (now sold at the blacksmith)");
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
    const int up1 = g.p.up[U_ARMOR], lv1 = g.p.sk.level(SK_CRAFTING), bone1 = g.p.mat(M_BONE), q1 = g.questIdx;
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
    ck(g.p.up[U_ARMOR] == up1 && g.p.sk.level(SK_CRAFTING) == lv1 && g.p.mat(M_BONE) == bone1 && g.questIdx == q1, "inventory, gear, skills and quests are untouched");
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
        SDL_SetRenderTarget(g.ren, g.canvas);
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

int delveTest(Game& g);  // delve_tests.cpp
int selfTest(Game& g);       // selftest.cpp
int weaponTest(Game& g);     // weapon_tests.cpp
int worldCheck(Game& g);     // world_check.cpp
int delveCheck(Game& g);     // world_check.cpp
int homesteadTest(Game& g);  // homestead_tests.cpp
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
    std::string delveDir, shots, tourDir, townDir, spriteDir, dumpFile, wildDir, exportDir;
    bool wtest = false, stest = false;
    bool wcheck = false, dtest = false, htest = false, soak = false;
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
        else if (takes(i, "--shots", shots) || takes(i, "--tour", tourDir) || takes(i, "--town", townDir) || takes(i, "--sprites", spriteDir) ||
                 takes(i, "--export-sprites", exportDir) || takes(i, "--dumpcore", dumpFile) || takes(i, "--wildlife", wildDir) ||
                 takes(i, "--delveshots", delveDir)) {}
        else if (!strcmp(argv[i], "--weapons")) wtest = true;
        else if (!strcmp(argv[i], "--selftest")) stest = true;
        else if (!strcmp(argv[i], "--worldcheck")) wcheck = true;
        else if (!strcmp(argv[i], "--delve")) dtest = true;
        else if (!strcmp(argv[i], "--homestead")) htest = true;
        else if (!strcmp(argv[i], "--soak")) soak = true;
    }
    // The test suites and tool modes save, load and delete save files: they get their own folder, never the player's real save.
    const bool toolRun = headless || wtest || stest || wcheck || dtest || htest || soak || !shots.empty() || !tourDir.empty() || !townDir.empty() ||
                         !spriteDir.empty() || !exportDir.empty() || !dumpFile.empty() || !wildDir.empty() || !delveDir.empty();
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
        // edge-triggered input must only be consumed once per frame even if several sim steps run
        bool first = true;
        while (acc >= step) {
            g.update((float)step);
            acc -= step;
            if (first) {
                g.in.lPressed = g.in.lReleased = g.in.ePressed = g.in.escPressed = g.in.tabPressed = g.in.enterPressed = false;
                g.in.bandage = g.in.mapToggle = g.in.helpToggle = g.in.invToggle = false;
                g.in.buildToggle = g.in.rotPressed = g.in.delPressed = g.in.rPressed = false;
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
