// game_logic.cpp - the simulation core: start-up, new game, effects, biomes / weather / camera, the per-frame update.
// (the rest of the simulation: game_player.cpp, game_animals.cpp, game_save.cpp, game_inventory.cpp)
#include <cstdio>
#include <fstream>
#include <sstream>
#include "core/game.hpp"
#include "core/logic_util.hpp"

// ================================================================= platform bits
bool Game::init(bool headless) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    win = SDL_CreateWindow("WILDWOOD HUNTER", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1200, 675, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!win) {
        fprintf(stderr, "window failed: %s\n", SDL_GetError());
        return false;
    }
    if (!headless) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        fprintf(stderr, "renderer failed: %s\n", SDL_GetError());
        return false;
    }
    canvas = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, LW, LH);
    buildAssets(ren, A);
    world.generate(WORLD_SEED);
    initHarvest();
    miniTex = toTex(ren, world.minimap());
    audio.init();
    SDL_ShowCursor(SDL_DISABLE);
    for (int i = 0; i < 110; i++) flakes.push_back({wrng.range(0, LW), wrng.range(0, LH), 0, 0, wrng.range(0, 6.28f), wrng.range(0.f, 1.f)});
    for (int i = 0; i < 30; i++) flies.push_back({rng.range(0, LW), rng.range(0, LH), rng.range(0, 6.28f), rng.range(0.6f, 1.4f)});
    recoverSave(savePath());
    std::ifstream t(savePath());
    hasSave = t.good();
    newGame();
    return true;
}

void Game::shutdown() {
    if (state != ST_TITLE) save();
    audio.shutdown();
    if (miniTex) SDL_DestroyTexture(miniTex);
    if (canvas) SDL_DestroyTexture(canvas);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
}

// ================================================================= helpers
void Game::newGame() {
    villagers.clear();
    villagersInit = false;
    resetDelveRuntime();
    for (auto& d : delveMem) d = DelveMem();
    for (auto& m : mapMem) m = MapMem();
    seen.assign((size_t)MM_W * MM_H, '0');
    travelFade = travelCd = 0;
    hsReset();
    interior = -1;
    svc = -1;
    craftBld = -1;
    extAnimals.clear();
    if (world.mapId != B_HOME) {  // a new game always starts in the original map (also leaves any building)
        structs.clear();
        loadMapWorld(B_HOME);
    }
    p = Player();
    if (ren && A.look != p.look) rebuildPlayerSprites(ren, A, p.look);
    p.pos = world.spawn;
    p.syncPack();
    p.inv.add(itemWpn(W_FLINT), 8);
    p.inv.add(ITEM_CONS0 + C_BANDAGE, 1);
    p.hot[0] = itemWpn(W_FLINT);          // the starting kit is equipped: a knife in the first weapon slot, the bandage in the first healing slot
    p.hot[3] = ITEM_CONS0 + C_BANDAGE;
    for (auto& st : structs) world.solid[st.ty][st.tx] = false;
    structs.clear();
    chestOpen = -1;
    craftCat = -1;
    craftScroll = 0;
    craftOnly = false;
    resetHarvest();  // every tree and plant is back
    animals.clear();
    items.clear();
    projs.clear();
    parts.clear();
    ftexts.clear();
    decals.clear();
    msgs.clear();
    dayT = 30;
    kills = 0;
    for (int& b : bestKills) b = 0;
    for (int b = 0; b < B_COUNT; b++) gateOpen[b] = b == B_HOME || b == B_HOMESTEAD;  // the four biomes start sealed
    gatesSeen = false;
    sold = 0;
    questIdx = 0;
    questProg = 0;
    questActive = false;
    refreshQuest();
    helpT = 0;  // the how-to card shows only on a first-ever start (see the title screen); H brings it back
    respawnT = 5;
    for (auto& l : world.landmarks) l.found = false;
    curBiome = pendBiome = B_HOME;
    pendT = bannerT = 0;
    for (int b = 0; b < B_COUNT; b++) bw[b] = b == B_HOME ? 1.f : 0.f;
    shakeAmt = 0;
    cam = p.pos - V2(LW / 2.f, LH / 2.f);
    populate();
    setCam(0, true);
}

float Game::tod() const { return fmodf(dayT / 300.f + 0.05f, 1.f); }
float Game::daylight() const { return smoothf(-0.25f, 0.35f, sinf(tod() * 6.2832f)); }

void Game::msg(const std::string& s, u32 col) {
    if (!msgs.empty() && msgs.back().s == s && msgs.back().col == col) {  // the same message again: count it instead of stacking it
        msgs.back().n++;
        msgs.back().t = 4.2f;
        return;
    }
    msgs.push_back({s, col, 4.2f});
    if (msgs.size() > 4) msgs.pop_front();
}
void Game::ftext(V2 pp, const std::string& s, u32 col, float life) { ftexts.push_back({pp, s, col, 0, life}); }
void Game::bloodBurst(V2 pp, V2 dir, int n) {
    for (int i = 0; i < n; i++) {
        float a = atan2f(dir.y, dir.x) + rng.range(-0.9f, 0.9f), sp = rng.range(30, 110);
        parts.push_back({pp.x, pp.y, rng.range(2, 8), cosf(a) * sp, sinf(a) * sp * 0.6f, rng.range(40, 110), 0.9f, 0.9f,
                         rng.chance(0.5f) ? HEX(0xb52a2e) : HEX(0x8c1d22), rng.chance(0.3f) ? 2 : 1, true});
    }
}
void Game::puff(V2 pp, int n, u32 col) {
    for (int i = 0; i < n; i++)
        parts.push_back({pp.x + rng.range(-3, 3), pp.y, rng.range(0, 3), rng.range(-14, 14), rng.range(-6, 6), rng.range(10, 36), 0.5f, 0.5f, col, 1, false});
}

Animal* Game::nearestCarcass(float maxD) {
    Animal* best = nullptr;
    float bd = maxD;
    for (auto& a : animals) {
        if (!a.dead || a.totalRemain <= 0 || a.gone) continue;
        float d = dist(p.pos, a.pos);
        if (d < bd) {
            bd = d;
            best = &a;
        }
    }
    return best;
}

// ================================================================= biomes
int Game::landmarksFound() const {
    int n = 0;
    for (auto& l : world.landmarks) n += l.found ? 1 : 0;
    return n;
}

// Stamina is the only survival stat, so climate works through it (and through terrain speed):
// heat saps you in daylight, cold bites unless you wear the fur coat, thin air and bog mud tire you out.
void Game::climateMods(float& drain, float& regen) const {
    drain = regen = 1.f;
    switch (curBiome) {
        case B_DESERT: {
            float heat = p.coolT > 0 ? 0.f : daylight() * (p.has(CG_DESERTWRAPS) ? 0.3f : 1.f);  // cactus water: the heat does not bother you
            drain = 1.f + 0.35f * heat;
            regen = 1.f - 0.35f * heat;
            break;
        }
        case B_SNOW: {
            float exposed = p.up[U_ARMOR] >= 2 ? 0.f : (p.up[U_ARMOR] == 1 ? 0.5f : 1.f);
            if (p.has(CG_PARKA) || p.has(CG_FROSTWARD)) exposed = 0.f;
            else if (p.has(CG_WOLFCLOAK)) exposed *= 0.5f;
            if (p.warmT > 0 || warmthAt(p.pos) > 0.3f) exposed = 0.f;  // a warm brew or standing by a campfire
            drain = 1.f + 0.25f * exposed;
            regen = 1.f - 0.30f * exposed;
            break;
        }
        case B_MOUNTAIN:
            if (p.has(CG_HARNESS)) drain = 1.08f, regen = 0.94f;  // climbing harness: thin air bites less
            else drain = 1.2f, regen = 0.85f;
            break;
        case B_SWAMP: drain = 1.1f; break;
        default: break;
    }
}

void Game::updateBiome(float dt) {
    int here = world.biomeAtPos(p.pos.x, p.pos.y);
    // hysteresis: a new biome must hold for half a second before it counts (borders are noisy)
    if (here != curBiome) {
        if (here != pendBiome) {
            pendBiome = here;
            pendT = 0;
        }
        pendT += dt;
        if (pendT >= 0.5f) {
            curBiome = here;
            bannerBiome = here;
            bannerT = 3.2f;
            bannerSub = BIOME[here].climate[0] ? std::string("CLIMATE: ") + BIOME[here].climate : std::string("");
            if (here != B_HOME) audio.play(S_PICKUP, 0.35f);
        }
    } else
        pendBiome = curBiome, pendT = 0;
    if (bannerT > 0) bannerT -= dt;
    float k = std::min(1.f, dt * 1.3f);
    for (int b = 0; b < B_COUNT; b++) bw[b] += ((b == here ? 1.f : 0.f) - bw[b]) * k;
    if (interior < 0 && delve < 0) revealAround(p.pos);  // fog of war on the minimap
    // discover landmarks by walking up to them
    for (auto& l : world.landmarks) {
        if (l.found || dist(p.pos, V2(l.x, l.y)) > 56) continue;
        l.found = true;
        msg(std::string("DISCOVERED: ") + l.name + " (" + S(landmarksFound()) + "/" + S((int)world.landmarks.size()) + ")", BIOME[l.biome].col);
        ftext(V2(l.x, l.y - 30), l.name, BIOME[l.biome].col, 2.2f);
        audio.play(S_PICKUP, 0.6f);
        grantXp(SK_SURVIVAL, 20);  // each landmark pays once (found is saved)
    }
}

// screen-space weather: each biome has its own particle behaviour; velocities blend by biome weight
void Game::updateWeather(float dt) {
    for (auto& f : flakes) {
        float vx = 0, vy = 0;
        float t = timeAcc * 0.6f + f.ph;
        // snow: falls and drifts
        vx += bw[B_SNOW] * (sinf(t * 1.7f) * 10.f + 9.f);
        vy += bw[B_SNOW] * (22.f + f.sz * 30.f);
        // desert: sand blown sideways in gusts
        float gust = 0.5f + 0.5f * sinf(timeAcc * 0.35f + f.ph * 0.2f);
        vx += bw[B_DESERT] * (45.f + 90.f * gust * (0.4f + f.sz));
        vy += bw[B_DESERT] * (sinf(t * 2.3f) * 6.f + 4.f);
        // swamp: gnats loiter
        vx += bw[B_SWAMP] * sinf(t * 2.1f) * 8.f;
        vy += bw[B_SWAMP] * cosf(t * 1.6f) * 7.f;
        // mountains: gusty wind dust
        vx += bw[B_MOUNTAIN] * (30.f + 55.f * gust);
        vy += bw[B_MOUNTAIN] * (sinf(t * 1.1f) * 5.f - 3.f);
        f.x += vx * dt;
        f.y += vy * dt;
        if (f.x > LW + 3) f.x = -3, f.y = wrng.range(0, LH);
        if (f.x < -3) f.x = LW + 3, f.y = wrng.range(0, LH);
        if (f.y > LH + 3) f.y = -3, f.x = wrng.range(0, LW);
        if (f.y < -3) f.y = LH + 3, f.x = wrng.range(0, LW);
    }
}

void Game::updateFx(float dt) {
    for (size_t i = 0; i < parts.size();) {
        Part& q = parts[i];
        q.life -= dt;
        q.x += q.vx * dt;
        q.y += q.vy * dt;
        q.z += q.vz * dt;
        if (q.grav) q.vz -= 260 * dt;
        if (q.z < 0) {
            q.z = 0;
            q.vx *= 0.4f;
            q.vy *= 0.4f;
            q.vz = 0;
            q.life -= dt * 2;
        }
        if (q.life <= 0) parts.erase(parts.begin() + i);
        else i++;
    }
    for (size_t i = 0; i < ftexts.size();) {
        ftexts[i].t += dt;
        ftexts[i].p.y -= 14 * dt;
        if (ftexts[i].t >= ftexts[i].life) ftexts.erase(ftexts.begin() + i);
        else i++;
    }
    for (size_t i = 0; i < decals.size();) {
        decals[i].t += dt;
        if (decals[i].t >= decals[i].life) decals.erase(decals.begin() + i);
        else i++;
    }
    for (size_t i = 0; i < items.size();) {
        items[i].t += dt;
        if (items[i].t > 300 && items[i].item < 0) items.erase(items.begin() + i);  // dropped stacks stay until picked up
        else i++;
    }
    for (auto& m : msgs) m.t -= dt;
    while (!msgs.empty() && msgs.front().t <= 0) msgs.pop_front();
    for (auto& f : flies) {
        f.x += (sinf(timeAcc * 0.7f * f.sp + f.ph) * 9 + 4) * dt;
        f.y += cosf(timeAcc * 0.9f * f.sp + f.ph * 2) * 7 * dt;
        if (f.x > LW + 4) f.x = -4;
        if (f.x < -4) f.x = LW + 4;
        if (f.y > LH + 4) f.y = -4;
        if (f.y < -4) f.y = LH + 4;
    }
    shakeAmt = std::max(0.f, shakeAmt - 16.f * dt);
}

void Game::setCam(float dt, bool snap) {
    if (interior >= 0) {  // a room is about one screen: the camera stays on its middle
        cam = world.roomCenter - V2(LW / 2.f, LH / 2.f);
        camX = (int)roundf(cam.x);
        camY = (int)roundf(cam.y);
        return;
    }
    V2 target = p.pos + V2(0, -8) - V2(LW / 2.f, LH / 2.f);
    if (snap) cam = target;
    else {
        float k = std::min(1.f, dt * 7.f);
        cam.x += (target.x - cam.x) * k;
        cam.y += (target.y - cam.y) * k;
    }
    cam.x = clampf(cam.x, 0, world.pixW() - LW);
    cam.y = clampf(cam.y, 0, world.pixH() - LH);
    float sx = 0, sy = 0;
    if (shakeAmt > 0.2f) {
        sx = rng.range(-1, 1) * shakeAmt * 0.6f;
        sy = rng.range(-1, 1) * shakeAmt * 0.6f;
    }
    camX = (int)roundf(cam.x + sx);
    camY = (int)roundf(cam.y + sy);
}

// Safety net against a soft-lock: if the hunter has no weapons, none lying on the ground or in
// flight, and can't afford a new flint pack even by selling everything, the merchant lends a few
// knives. Checked about once a second; it never triggers for a player who can still recover.
void Game::checkWeaponSafetyNet(float dt) {
    stuckCheckT -= dt;
    if (stuckCheckT > 0) return;
    stuckCheckT = 1.f;
    if (p.dead || p.charging) return;
    for (int w = 0; w < W_COUNT; w++)
        if (p.wpn(w) > 0 && (!isLauncher(w) || p.ammo(WPN[w].ammo) > 0)) return;  // still holding a weapon (a bow with no arrows doesn't count)
    if (!projs.empty() || !items.empty()) return;  // a thrown weapon can still be picked up
    int wealth = p.coins;
    for (int m = 0; m < M_COUNT; m++) wealth += p.mat(m) * MAT_PRICE[m];
    if (wealth >= FLINT_PACK_PRICE) return;  // could buy flint at the shop
    if (p.inv.slotRoom(itemWpn(W_FLINT)) < FLINT_LOAN) {  // all slots used: make room by dropping the cheapest material stack
        int cheapest = -1;
        for (int m = 0; m < M_COUNT; m++)
            if (p.mat(m) > 0 && (cheapest < 0 || MAT_PRICE[m] < MAT_PRICE[cheapest])) cheapest = m;
        if (cheapest >= 0) p.inv.remove(itemMat(cheapest), p.mat(cheapest));
    }
    p.inv.addIgnoringWeight(itemWpn(W_FLINT), FLINT_LOAN);  // the loan is never refused for weight
    if (p.hotSlotOf(itemWpn(W_FLINT)) < 0) {  // and it is equipped, so the hunter can use it at once
        int slot = 0;
        for (int i = HOT_N - 1; i >= 0; i--)
            if (p.hot[i] < 0 || i == 0) slot = i;
        p.hot[slot] = itemWpn(W_FLINT);
    }
    p.sel = W_FLINT;
    msg("THE MERCHANT LENDS YOU " + S(FLINT_LOAN) + " FLINT KNIVES", C_GOLD);
    ftext(p.pos + V2(0, -28), "+" + S(FLINT_LOAN) + " FLINT KNIFE", C_BLUE, 1.4f);
    audio.play(S_PICKUP);
}

void Game::updatePlay(float dt) {
    dayT += dt;
    updateLight(dt);  // storms and the lamp list (game_light.cpp)
    if (bldCd > 0) bldCd -= dt;
    if (helpT > 0) helpT -= dt;
    if (bagWarn > 0) bagWarn -= dt;
    updateHomestead(dt);
    if (bm.on && inHomestead()) updateBuildMode();
    if (delve < 0) updateBiome(dt);
    updatePlayer(dt);
    updateHarvest(dt);
    updateStructures(dt);
    p.warmT = std::max(0.f, p.warmT - dt);
    p.coolT = std::max(0.f, p.coolT - dt);
    p.swiftT = std::max(0.f, p.swiftT - dt);
    p.nightT = std::max(0.f, p.nightT - dt);
    updateAnimals(dt);
    updateVillagers(dt);
    updateElder(dt);
    updateShopTalk(dt);
    updateProjectiles(dt);
    tickDelveTimers(dt);
    if (delve >= 0) updateDelve(dt);
    checkWeaponSafetyNet(dt);
    updateFx(dt);
    updateWeather(dt);
    setCam(dt);
    // keep the wilds populated
    respawnT -= dt;
    if (respawnT <= 0 && (interior >= 0 || delve >= 0 || world.biomeAtPos(p.pos.x, p.pos.y) != B_HOME)) respawnT = 4.f;  // home herds only refill while you are home
    if (respawnT <= 0) {
        respawnT = 4.f;
        int alive[A_COUNT] = {0};
        for (auto& a : animals)
            if (!a.dead) alive[a.kind]++;
        int targets[4] = {HOME_HERDS[0], HOME_HERDS[1], HOME_HERDS[2], HOME_HERDS[3] + (int)(3 * (1.f - daylight()))};
        for (int k = 0; k < 4; k++)  // the original four species keep their home-area ring population
            if (alive[k] < targets[k]) trySpawn(k, 230, true);
    }
    if (interior < 0 && delve < 0) streamSpawns(dt);
    checkPortals(dt);  // last: a trip replaces the world, so nothing above may run on it afterwards
}

// ---- ambience and footsteps (sound beds are synthesised in audio.hpp)
int Game::ambienceFor(float& vol) const {
    int kind = AMB_NONE;
    vol = 1.f;
    const bool night = interior < 0 && daylight() < 0.35f;
    if (world.isDelve()) kind = AMB_CAVE;
    else if (interior >= 0) kind = AMB_NONE;  // a room is quiet
    else {
        switch (inHomestead() ? (int)B_HOME : curBiome) {
            case B_DESERT: kind = AMB_DESERT; break;
            case B_SWAMP: kind = AMB_SWAMP; break;
            case B_SNOW: kind = AMB_SNOW; break;
            case B_MOUNTAIN: kind = AMB_MOUNTAIN; break;
            default: kind = night ? AMB_NIGHT : AMB_DAY; break;
        }
        if (night && kind == AMB_DESERT) vol = 0.6f;
    }
    return kind;
}

void Game::updateAmbience() {
    float vol = 1.f;
    const int kind = ambienceFor(vol);
    audio.setAmbience(kind, state == ST_PAUSE ? vol * 0.4f : vol);
}

void Game::playFootstep() {
    Sfx s = S_STEP_GRASS;
    switch (world.tileAt(p.pos.x, p.pos.y)) {
        case T_STONE: case T_COBBLE: case T_FLOORS: case T_FLOORT: case T_CFLOOR: case T_RAIL: case T_IFLOOR: case T_BRICK: case T_ICE: s = S_STEP_STONE; break;
        case T_FLOORW: s = S_STEP_WOOD; break;
        case T_SAND: case T_DUNE: s = S_STEP_SAND; break;
        case T_SNOW: case T_SNOWP: s = S_STEP_SNOW; break;
        case T_MUD: case T_MURK: s = S_STEP_MUD; break;
        default: break;
    }
    audio.play(s, p.charging ? 0.12f : 0.22f);
}

void Game::update(float dt) {
    timeAcc += dt;
    if (state == ST_TITLE) audio.setAmbience(AMB_NONE, 0.f);
    else updateAmbience();
    switch (state) {
        case ST_TITLE: {
            titleT += dt;
            dayT += dt * 0.6f;
            updateAnimals(dt);
            updateFx(dt);
            V2 c = world.village + V2(cosf(titleT * 0.12f) * 110, sinf(titleT * 0.12f) * 55 + 10);
            cam = c - V2(LW / 2.f, LH / 2.f);
            cam.x = clampf(cam.x, 0, world.pixW() - LW);
            cam.y = clampf(cam.y, 0, world.pixH() - LH);
            camX = (int)roundf(cam.x);
            camY = (int)roundf(cam.y);
            if (in.enterPressed) {
                const bool first = !hasSave;
                newGame();
                if (first) helpT = 16;
                state = ST_PLAY;
                audio.play(S_CLICK);
            }
            break;
        }
        case ST_PLAY:
            if (in.escPressed && bm.on) {
                bm.on = false;
                break;
            }
            if (in.escPressed) {
                state = ST_PAUSE;
                break;
            }
            if (in.buildToggle && !p.dead && !bm.on) {
                if (inHomestead() && interior < 0) {
                    state = ST_BUILD;
                    audio.play(S_CLICK);
                } else msg("YOU CAN ONLY BUILD AT YOUR HOMESTEAD", C_DIM);
                break;
            }
            if (in.mapToggle) {  // M: the full map; the minimap on / off is TAB
                state = ST_MAP;
                audio.play(S_CLICK);
                break;
            }
            if (in.tabPressed) showMap = !showMap;
            if (in.helpToggle) helpT = helpT > 0 ? 0 : 14;
            if (in.invToggle && !p.dead && !p.charging) {
                state = ST_INVENTORY;
                invSel = std::max(0, std::min(invSel, p.bagCap() - 1));
                for (int i = 0; i < 4; i++) navPrev[i] = true;  // keys held while opening must not move the cursor
                audio.play(S_CLICK);
                break;
            }
            updatePlay(dt);
            break;
        case ST_INVENTORY:
            updateInventory(dt);
            break;
        case ST_MAP:
            updateWorldMap(dt);
            break;
        case ST_SHOP:
            dayT += 0;  // time stands still while trading
            updateFx(dt);
            if (in.escPressed || in.ePressed) {
                state = ST_PLAY;
                save();
                audio.play(S_CLICK);
            }
            if (in.tabPressed) shopTab = (shopTab + 1) % 2;
            if (shopTab == 0) sellScroll -= in.wheel;
            if (in.selectKey >= 0 && in.selectKey < 2) shopTab = in.selectKey;
            break;
        case ST_CHEST:
            updateChest(dt);
            break;
        case ST_SERVICE:
            updateService(dt);
            break;
        case ST_BUILD: updateBuildMenu(dt); break;
        case ST_FARM: updateFarmMenu(dt); break;
        case ST_ANIMAL: updateAnimalMenu(dt); break;
        case ST_MARKET: updateMarket(dt); break;
        case ST_PAUSE:
            if (in.escPressed) state = ST_PLAY;
            break;
        case ST_GATE: updateGatePanel(dt); break;
        case ST_QUEST:  // time stands still while talking to the elder
            updateFx(dt);
            if (in.escPressed || in.ePressed) {
                state = ST_PLAY;
                save();
                audio.play(S_CLICK);
            }
            break;
    }
}
