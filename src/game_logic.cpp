// game_logic.cpp - simulation: player, animals, projectiles, gathering, economy, save/load.
#include <cstdio>
#include <fstream>
#include <sstream>
#include "game.hpp"

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float smoothf(float a, float b, float x) {
    float t = clampf((x - a) / (b - a), 0, 1);
    return t * t * (3 - 2 * t);
}

// move with axis-separated tile collision, in small steps so nothing tunnels
static void moveEnt(const World& w, V2& pos, V2 d, float r) {
    int steps = (int)ceilf(std::max(fabsf(d.x), fabsf(d.y)) / 3.f);
    if (steps < 1) steps = 1;
    V2 s = d * (1.f / steps);
    for (int i = 0; i < steps; i++) {
        if (!w.blocked(pos.x + s.x, pos.y, r)) pos.x += s.x;
        if (!w.blocked(pos.x, pos.y + s.y, r)) pos.y += s.y;
    }
}

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

std::string Game::savePath() {
    char* b = SDL_GetPrefPath("WildwoodGames", "WildwoodHunter");
    std::string s = b ? std::string(b) + "save.txt" : std::string("wildwood_save.txt");
    if (b) SDL_free(b);
    return s;
}

// ---- saving. Format version 2 stores the inventory as "slot <item key> <count>" lines, one per used slot, using the item's
// stable name (see itemKey). Version 1 saves (no "version" line) had separate mats / wpn / ammo / bandages lines; they are
// migrated on load, and the original file is copied to save.v1.bak the first time that happens.
constexpr int SAVE_VERSION = 2;

void Game::save() {
    const std::string path = savePath(), tmp = path + ".tmp";
    {
        std::ofstream f(tmp);
        if (!f) return;
        f << "version " << SAVE_VERSION << "\ncoins " << p.coins << "\nup";
        for (int i = 0; i < U_COUNT; i++) f << ' ' << p.up[i];
        f << "\nday " << dayT << "\nkills " << kills << "\n";
        f << "quest " << questIdx << ' ' << (questActive ? 1 : 0) << ' ' << questProg << "\n";
        skillSave(f);
        for (auto& sl : p.inv.slots)
            if (!sl.empty()) f << "slot " << itemKey(sl.item) << ' ' << sl.n << "\n";
        // maps: "at <map>" is where the hunter is; each visited map then has a "map <id>" block with its discovered landmarks
        // ("disc"), the resources waiting to regrow ("hv") and its campfires / chests ("struct", "cslot"). Extra lines that older
        // builds simply ignore, so the save version stays 2.
        snapshotMap();
        f << "at " << world.mapId << "\n";
        for (int m = 0; m < B_COUNT; m++) {
            const MapMem& mm = mapMem[m];
            if (!mm.visited) continue;
            f << "map " << m << "\ndisc " << mm.disc << "\n";
            for (auto& ln : mm.hv) f << ln << "\n";
            for (auto& ln : mm.st) f << ln << "\n";
        }
        f.flush();
        if (!f) {
            std::remove(tmp.c_str());
            return;
        }
    }
    std::remove(path.c_str());
    std::rename(tmp.c_str(), path.c_str());  // write-then-rename: a crash mid-save cannot leave a half-written file
    hasSave = true;
}

bool Game::load() {
    std::ifstream f(savePath());
    if (!f) return false;
    newGame();
    loadReport = LoadReport();
    auto readCount = [](std::istringstream& ss, long lim = 99999) {
        long v = 0;
        if (!(ss >> v)) return 0L;
        return std::max(0L, std::min(lim, v));
    };
    std::vector<std::pair<std::string, int>> slotLines;  // v2
    // per-map state read from the file: "map <id>" starts a block; lines before any block come from a save made before the
    // separate maps existed (one big world) and are converted below
    std::vector<std::string> mapHv[B_COUNT], mapSt[B_COUNT];
    std::string mapDisc[B_COUNT];
    std::vector<std::string> legHv, legSt;
    std::string legDisc;
    std::vector<std::pair<std::string, int>> legacyRefund;  // items from old out-of-home chests (see below)
    int curLoad = -1, atMap = B_HOME;
    bool sawMapBlock = false;
    int legacyMats[M_COUNT] = {0}, legacyWpn[W_COUNT] = {0}, legacyAmmo[AM_COUNT] = {0}, legacyBandages = 0;
    int nMats = 0, nWpn = 0, nAmmo = 0;
    bool sawVersion = false, sawLegacy = false;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string k;
        ss >> k;
        if (k == "version") {
            ss >> loadReport.version;
            sawVersion = true;
        } else if (k == "coins") p.coins = (int)readCount(ss, 100000000);
        else if (k == "slot") {
            std::string key;
            ss >> key;
            slotLines.push_back({key, (int)readCount(ss)});
        } else if (k == "map") {
            ss >> curLoad;
            if (!ss || curLoad < 0 || curLoad >= B_COUNT) curLoad = -2;  // an unknown map: its lines are skipped
            sawMapBlock = true;
        } else if (k == "at") {
            ss >> atMap;
            if (!ss || atMap < 0 || atMap >= B_COUNT) atMap = B_HOME;
        } else if (k == "hv") {
            if (curLoad >= 0) mapHv[curLoad].push_back(line);
            else if (curLoad == -1) legHv.push_back(line);
        } else if (k == "struct" || k == "cslot") {
            if (curLoad >= 0) mapSt[curLoad].push_back(line);
            else if (curLoad == -1) legSt.push_back(line);
        } else if (k == "mats") {
            sawLegacy = true;
            for (; nMats < M_COUNT && !ss.eof(); nMats++) legacyMats[nMats] = (int)readCount(ss);
        } else if (k == "wpn") {
            sawLegacy = true;
            for (; nWpn < W_COUNT && !ss.eof(); nWpn++) legacyWpn[nWpn] = (int)readCount(ss);
        } else if (k == "ammo") {
            sawLegacy = true;
            for (; nAmmo < AM_COUNT && !ss.eof(); nAmmo++) legacyAmmo[nAmmo] = (int)readCount(ss);
        } else if (k == "bandages") {
            sawLegacy = true;
            legacyBandages = (int)readCount(ss);
        } else if (k == "up") for (int i = 0; i < U_COUNT; i++) ss >> p.up[i];
        else if (k == "day") ss >> dayT;
        else if (k == "kills") ss >> kills;
        else if (k == "skill") skillLoadLine(ss);
        else if (k == "disc") {
            std::string bits;
            ss >> bits;
            if (curLoad >= 0) mapDisc[curLoad] = bits;
            else if (curLoad == -1) legDisc = bits;
        } else if (k == "quest") {  // "quest <index> <active 0/1> <progress>"; older saves simply lack this line
            int active = 0;
            ss >> questIdx >> active >> questProg;
            questActive = active != 0;
        }
    }
    if (!sawVersion && !sawLegacy) {  // not a save file we understand: say so instead of wiping the hunter's kit
        loadReport.version = 0;
        newGame();
        return false;
    }
    for (int i = 0; i < U_COUNT; i++) p.up[i] = std::min(UP_MAX[i], std::max(0, p.up[i]));  // the top levels (scale mail, reinforced leather, bear hide, swiftfoot boots, explorer pack) are craft-only
    p.syncPack();  // the backpack level decides how many slots there are, so it must be known before items go in
    p.inv.clear();
    auto give = [&](int item, int n) {
        if (n <= 0) return;
        int got = p.inv.add(item, n);
        loadReport.dropped += n - got;
    };
    if (sawVersion) {
        for (auto& sl : slotLines) {
            int id = itemFromKey(sl.first);
            if (id < 0) {
                loadReport.unknown++;  // an item from a newer game version: skipped, the rest still loads
                continue;
            }
            give(id, sl.second);
        }
    } else {
        // version 1: counters per category. Old saves may hold fewer materials / weapons than exist now; the missing ones are zero.
        loadReport.version = 1;
        loadReport.migrated = true;
        for (int m = 0; m < M_COUNT; m++) give(itemMat(m), legacyMats[m]);
        for (int w = 0; w < W_COUNT; w++) give(itemWpn(w), legacyWpn[w]);
        for (int a = 0; a < AM_COUNT; a++) give(itemAmmo(a), legacyAmmo[a]);
        give(ITEM_CONS0 + C_BANDAGE, legacyBandages);
        const std::string bak = savePath() + ".v1.bak";
        std::ifstream have(bak);
        if (!have) {
            std::ifstream src(savePath(), std::ios::binary);
            std::ofstream dst(bak, std::ios::binary);
            dst << src.rdbuf();
        }
    }
    refreshQuest();
    // A save from before the separate maps stored everything in one 240x168 world with the original area at tile (76,54), which is
    // exactly where it is in the new home map. Data inside that area is kept as it was. Structures out in the old biome areas cannot
    // stay where they were (that ground is now home forest or another map), so their
    // campfires / chests and everything inside them go back to the pack instead of being lost.
    if (!sawMapBlock && (!legHv.empty() || !legSt.empty() || !legDisc.empty())) {
        const int DN[4] = {4, 3, 3, 4};  // old home offset; landmarks per biome (desert, swamp, snow, mountain)
        for (auto& ln : legHv) {
            std::istringstream ss(ln);
            std::string tag;
            char kind;
            int tx, ty, res, secs;
            if (!(ss >> tag >> kind >> tx >> ty >> res >> secs)) continue;
            if (tx >= COX && ty >= COY && tx < COX + CW && ty < COY + CH) mapHv[B_HOME].push_back("hv " + std::string(1, kind) + " " + S(tx) + " " + S(ty) + " " + S(res) + " " + S(secs));
        }
        std::vector<int> newIdx;  // old struct index -> index in the home map, or -1 when it was out in a biome
        std::vector<int> oldKind;
        for (auto& ln : legSt) {
            std::istringstream ss(ln);
            std::string tag;
            ss >> tag;
            if (tag != "struct") continue;
            int kind = -1, tx = 0, ty = 0;
            ss >> kind >> tx >> ty;
            oldKind.push_back(kind);
            if (ss && tx >= COX && ty >= COY && tx < COX + CW && ty < COY + CH) {
                newIdx.push_back((int)mapSt[B_HOME].size() - 0);
                int count = 0;
                for (auto& e : mapSt[B_HOME]) count += e.compare(0, 7, "struct ") == 0 ? 1 : 0;
                newIdx.back() = count;
                mapSt[B_HOME].push_back("struct " + S(kind) + " " + S(tx) + " " + S(ty));
            } else
                newIdx.push_back(-1);
        }
        for (size_t i = 0; i < newIdx.size(); i++)
            if (newIdx[i] < 0 && oldKind[i] >= 0 && oldKind[i] < PL_COUNT) legacyRefund.push_back({itemKey(itemPlace(oldKind[i])), 1});
        for (auto& ln : legSt) {
            std::istringstream ss(ln);
            std::string tag, key;
            int idx = -1, n = 0;
            ss >> tag >> idx >> key >> n;
            if (tag != "cslot" || !ss || idx < 0 || idx >= (int)newIdx.size()) continue;
            if (newIdx[idx] >= 0) mapSt[B_HOME].push_back("cslot " + S(newIdx[idx]) + " " + key + " " + S(n));
            else legacyRefund.push_back({key, n});
        }
        int at = 0;
        for (int b = B_DESERT; b <= B_MOUNTAIN; at += DN[b - B_DESERT], b++)
            mapDisc[b] = at < (int)legDisc.size() ? legDisc.substr(at, DN[b - B_DESERT]) : std::string();
    }
    for (auto& rf : legacyRefund) {  // (see above) nothing a hunter owned may vanish
        const int id = itemFromKey(rf.first);
        if (id >= 0) loadReport.dropped += rf.second - p.inv.add(id, rf.second);
    }
    for (int m = 0; m < B_COUNT; m++) {
        mapMem[m].hv = mapHv[m];
        mapMem[m].st = mapSt[m];
        mapMem[m].disc = mapDisc[m];
        mapMem[m].visited = !mapHv[m].empty() || !mapSt[m].empty() || !mapDisc[m].empty() || m == atMap;
    }
    restoreMap();  // the home map is active after newGame(); other maps are put back when you travel there
    p.maxhp = (float)ARMOR_HP[p.up[U_ARMOR]];
    p.hp = p.maxhp;
    // keep a usable weapon selected
    if (!canUse(p.sel))
        for (int w = 0; w < W_COUNT; w++)
            if (canUse(w)) {
                p.sel = w;
                break;
            }
    if (atMap != B_HOME) travelTo(atMap, MAPS[atMap].entryTx, MAPS[atMap].entryTy, false);  // saved out in a biome: continue there
    return true;
}

// ================================================================= helpers
void Game::newGame() {
    for (auto& m : mapMem) m = MapMem();
    travelFade = travelCd = 0;
    if (world.mapId != B_HOME) {  // a new game always starts in the original map
        structs.clear();
        loadMapWorld(B_HOME);
    }
    p = Player();
    p.pos = world.spawn;
    p.syncPack();
    p.inv.add(itemWpn(W_FLINT), 8);
    p.inv.add(ITEM_CONS0 + C_BANDAGE, 1);
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
    sold = 0;
    questIdx = 0;
    questProg = 0;
    questActive = false;
    refreshQuest();
    helpT = 16;
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
bool Game::nearShop() const { return dist(p.pos, world.keeper) < 44 || dist(p.pos, world.door) < 36; }

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

// ================================================================= animals
bool Game::trySpawn(int kind, float minDist, bool offscreen) {
    static const float zr[A_COUNT][2] = {{140, 520}, {240, 700}, {360, 820}, {460, 900}};
    for (int it = 0; it < 80; it++) {
        float ang = rng.range(0, 6.2832f), rad = rng.range(zr[kind][0], zr[kind][1]);
        V2 pos = world.village + V2(cosf(ang) * rad, sinf(ang) * rad * 0.8f);
        // the original species still live in the home area only, with the same margins as before the expansion
        if (pos.x < world.coreX0() + 56 || pos.y < world.coreY0() + 56 || pos.x > world.coreX1() - 56 || pos.y > world.coreY1() - 56) continue;
        if (world.blocked(pos.x, pos.y, 9)) continue;
        if (dist(pos, p.pos) < minDist) continue;
        if (offscreen) {
            float sx = pos.x - cam.x, sy = pos.y - cam.y;
            if (sx > -50 && sx < LW + 50 && sy > -60 && sy < LH + 60) continue;
        }
        Animal a;
        a.kind = kind;
        a.biome = world.biomeAtPos(pos.x, pos.y);
        a.variant = (kind == A_DEER && rng.chance(0.45f)) ? 1 : 0;
        a.pos = pos;
        a.target = pos;
        applyTier(a, rollTier());
        a.face = rng.chance(0.5f) ? 1 : -1;
        a.bias = rng.chance(0.5f) ? 1.f : -1.f;
        a.idleT = rng.range(0, 3);
        a.animT = rng.range(0, 4);
        a.bodyH = ANIMAL[kind].bodyH * a.scale;
        animals.push_back(a);
        return true;
    }
    return false;
}

// ---- rarity tiers. Rolled on frng so the gameplay rng sequence is the same as before the wildlife update.
int Game::rollTier() {
    float u = frng.f();
    float c = TIER[TR_LEGENDARY].chance;
    if (u < c && liveLegendaries() < LEGEND_MAX_ALIVE && legCool <= 0) {
        legCool = LEGEND_COOLDOWN;
        return TR_LEGENDARY;
    }
    // a legendary roll that is not allowed (one already alive / cooling down) simply becomes a rare
    if (u < (c += TIER[TR_RARE].chance)) return TR_RARE;
    if (u < c + TIER[TR_UNCOMMON].chance) return TR_UNCOMMON;
    return TR_COMMON;
}
void Game::applyTier(Animal& a, int tier) {
    const TierDef& T = TIER[tier];
    a.tier = tier;
    spawnStat[a.kind][tier]++;
    a.scale = T.scale;
    a.maxhp = a.hp = std::max(1, (int)roundf(ANIMAL[a.kind].hp * T.hp));
}
int Game::liveLegendaries() const {
    int n = 0;
    for (auto& a : animals)
        if (!a.dead && a.tier == TR_LEGENDARY) n++;
    return n;
}

Animal* Game::spawnAnimal(int kind, V2 pos, int form, int tier) {
    Animal a;
    const AnimalDef& D = ANIMAL[kind];
    a.kind = kind;
    a.biome = world.biomeAtPos(pos.x, pos.y);
    a.variant = form;
    a.pos = a.target = pos;
    applyTier(a, tier);
    a.face = frng.chance(0.5f) ? 1 : -1;
    a.bias = frng.chance(0.5f) ? 1.f : -1.f;
    a.idleT = frng.range(0, 3);
    a.animT = frng.range(0, 4);
    a.bodyH = D.bodyH * a.scale;
    if (D.flags & BH_FLIES) a.air = 13.f;
    animals.push_back(a);
    return &animals.back();
}

static bool nearDeepWater(const World& w, int tx, int ty, int r) {
    for (int y = ty - r; y <= ty + r; y++)
        for (int x = tx - r; x <= tx + r; x++)
            if (w.water(x, y)) return true;
    return false;
}

// Streaming spawns for the four new biomes: keeps the live population small and local.
void Game::streamSpawns(float dt) {
    for (int k = 0; k < A_COUNT; k++)
        if (spCool[k] > 0) spCool[k] -= dt;
    if (legCool > 0) legCool -= dt;
    spawnT -= dt;
    if (spawnT > 0) return;
    spawnT = SPAWN_TICK;
    // despawn: far away living animals (never carcasses - those fade on their own)
    for (auto& a : animals)
        if (!a.dead && a.biome != B_HOME && dist(a.pos, p.pos) > DESPAWN_DIST) a.gone = true;
    int liveN = 0;
    for (auto& a : animals)
        if (!a.dead && !a.gone && a.biome != B_HOME) liveN++;  // the home population has its own fixed targets
    if (liveN >= MAX_LIVE_ANIMALS || p.dead) return;
    int b = world.biomeAtPos(p.pos.x, p.pos.y);
    if (b == B_HOME) return;
    float night = 1.f - daylight();
    // pick one due species per tick (round-robin start so the table order doesn't bias)
    const auto& tab = SPAWNS[b];
    if (tab.empty()) return;
    int start = frng.irange(0, (int)tab.size() - 1);
    for (size_t j = 0; j < tab.size(); j++) {
        const SpawnDef& S = tab[(start + j) % tab.size()];
        if (spCool[S.kind] > 0) continue;
        int cnt = 0;
        for (auto& a : animals)
            if (!a.dead && !a.gone && a.kind == S.kind && dist(a.pos, p.pos) < SPAWN_AREA) cnt++;
        int cap = S.cap + (night > 0.5f ? (int)S.capNight : 0);
        if (cnt >= cap) continue;
        if (!frng.chance(S.chance)) {
            spCool[S.kind] = 2.f;  // failed roll: try again soon, not on the next tick
            continue;
        }
        for (int it = 0; it < 30; it++) {
            float ang = frng.range(0, 6.2832f), rad = frng.range(SPAWN_MIN, SPAWN_MAX);
            V2 pos = p.pos + V2(cosf(ang) * rad, sinf(ang) * rad * 0.8f);
            if (world.biomeAtPos(pos.x, pos.y) != b) continue;
            if (world.blocked(pos.x, pos.y, 9)) continue;
            int tx = (int)(pos.x / TS), ty = (int)(pos.y / TS);
            if (S.terrain && !(S.terrain & TM(world.tile[ty][tx]))) continue;
            if (S.nearWater && !nearDeepWater(world, tx, ty, 3)) continue;
            float sx = pos.x - cam.x, sy = pos.y - cam.y;
            if (sx > -50 && sx < LW + 50 && sy > -60 && sy < LH + 60) continue;
            int form = (ANIMAL[S.kind].forms > 1 && frng.chance(S.form1)) ? 1 : 0;
            spawnAnimal(S.kind, pos, form, rollTier());
            spCool[S.kind] = S.cooldown;
            return;
        }
        spCool[S.kind] = 2.f;
        return;
    }
}

void Game::populate() {
    animals.clear();
    int counts[A_COUNT] = {14, 8, 5, 4};
    for (int k = 0; k < A_COUNT; k++)
        for (int i = 0; i < counts[k]; i++) trySpawn(k, 110, false);
}

bool Game::stepAnimal(Animal& a, V2 dir, float speed, float dt) {
    static const float offs[7] = {0, 0.6f, -0.6f, 1.2f, -1.2f, 1.9f, -1.9f};
    float base = atan2f(dir.y, dir.x);
    const AnimalDef& D = ANIMAL[a.kind];
    float r = D.hop ? 3.5f : 5.5f;
    // rabbits move in hops
    float sp = speed;
    if (D.hop) sp *= 0.35f + 0.9f * std::max(0.f, sinf(a.animT * 1.5708f * 2));
    for (int k = 0; k < 7; k++) {
        float ang = base + offs[k] * a.bias;
        V2 d(cosf(ang), sinf(ang));
        V2 np = a.pos + d * (sp * dt);
        if (!world.blocked(np.x, np.y, r) && world.biomeAtPos(np.x, np.y) == a.biome) {
            if (k > 0 && (k % 2 == 0)) a.bias = -a.bias;
            a.pos = np;
            a.vel = d * sp;
            return true;
        }
    }
    a.vel = V2();
    return false;
}

void Game::updateAnimals(float dt) {
    float night = 1.f - daylight();
    for (auto& a : animals) {
        if (a.flash > 0) a.flash -= dt;
        if (a.hpShow > 0) a.hpShow -= dt;
        if (a.atkCd > 0) a.atkCd -= dt;
        if (a.kb.len() > 3) {
            moveEnt(world, a.pos, a.kb * dt, 5.f);
            a.kb = a.kb * expf(-9 * dt);
        } else
            a.kb = V2();
        if (a.dead) {
            a.deadT += dt;
            if (a.deadT > 170) a.fade -= dt / 8.f;
            if (a.totalRemain <= 0) a.fade -= dt * 1.6f;
            if (a.fade <= 0) a.gone = true;
            continue;
        }
        const AnimalDef& D = ANIMAL[a.kind];
        V2 dp = p.pos - a.pos;
        float dd = p.dead ? 9999.f : dp.len();
        const TierDef& T = TIER[a.tier];
        // night hunters (cats, coyotes) are skittish in daylight
        bool agg = D.aggro && (!(D.flags & BH_NIGHT_AGGRO) || night > 0.45f);
        float notice = D.notice * T.notice * (D.aggro ? 1.f + 0.3f * night : 1.f);
        a.stT -= dt;
        a.vel = V2();
        switch (a.st) {
            case AS_WANDER:
            case AS_FEED:
            case AS_REST: {
                bool alert = dd < (D.aggro ? notice * D.alertMul : notice);
                if (a.st != AS_WANDER) {
                    if (a.st == AS_REST) a.vel = V2();
                    if (!alert && a.stT <= 0) {
                        a.st = AS_WANDER;
                        a.idleT = frng.range(0.5f, 2.f);
                    }
                    if (!alert) break;
                }
                if (alert) {
                    if (agg) {
                        a.st = AS_CHASE;
                        a.stT = 12;
                        ftext(a.pos + V2(0, -a.bodyH - 12), "!", C_RED, 0.8f);
                    } else {
                        a.st = AS_FLEE;
                        a.stT = rng.range(2.5f, 4.f);
                        ftext(a.pos + V2(0, -a.bodyH - 12), "!", C_TEXT, 0.7f);
                    }
                    break;
                }
                if (a.idleT > 0) {
                    a.idleT -= dt;
                    if (a.idleT <= 0 && D.restChance > 0 && frng.chance(D.restChance + D.feedChance)) {
                        bool rest = frng.chance(D.restChance / (D.restChance + D.feedChance));
                        a.st = rest ? AS_REST : AS_FEED;
                        a.stT = frng.range(4.f, 9.f);
                        a.idleT = 0;
                    }
                } else {
                    V2 to = a.target - a.pos;
                    if (to.len() < 5 || a.stT < -7) {
                        a.idleT = rng.range(1.2f, 4.5f);
                        a.target = a.pos + V2(rng.range(-90, 90), rng.range(-70, 70));
                        a.stT = 0;
                    } else if (!stepAnimal(a, to.norm(), D.wander * T.speed, dt)) {
                        a.target = a.pos + V2(rng.range(-60, 60), rng.range(-50, 50));
                    }
                }
                break;
            }
            case AS_FLEE: {
                V2 away = (a.pos - p.pos).norm();
                if (away.len() < 0.1f) away = V2(1, 0);
                V2 perp(-away.y, away.x);
                float j = sinf(timeAcc * 2.3f + a.animT) * 0.35f;
                V2 dir = (away + perp * j).norm();
                stepAnimal(a, dir, D.run * T.speed * (a.hp < a.maxhp ? 1.08f : 1.f), dt);
                if (a.stT <= 0 && dd > notice * 1.4f) {
                    a.st = AS_WANDER;
                    a.idleT = 1;
                    a.target = a.pos;
                }
                break;
            }
            case AS_CHASE: {
                V2 toP = dp.norm();
                if (D.fleeHp > 0 && a.hp < a.maxhp * D.fleeHp) {
                    a.st = AS_FLEE;
                    a.stT = 6;
                    break;
                }
                if (dd > D.leash || p.dead || !agg) {
                    a.st = AS_WANDER;
                    a.idleT = 1.5f;
                    a.target = a.pos;
                    break;
                }
                a.face = dp.x > 0 ? 1 : -1;
                if (dd < D.hitR + 9) {
                    if (a.atkCd <= 0) {
                        hurtPlayer(D.dmg * T.dmg, a.pos, a.kind);
                        a.atkCd = D.atkCd;
                        a.kb = toP * -70;
                    }
                } else
                    stepAnimal(a, toP, D.run * T.speed * (dd < 50 ? 1.12f : 1.f), dt);
                break;
            }
        }
        if (fabsf(a.vel.x) > 2.f && a.st != AS_CHASE) a.face = a.vel.x > 0 ? 1 : -1;
        else if (fabsf(a.vel.x) > 2.f) a.face = a.vel.x > 0 ? 1 : -1;
        float spd = a.vel.len();
        if (spd > 1) a.animT += spd * dt * D.animK;
        a.noticeT += dt;
    }
    animals.erase(std::remove_if(animals.begin(), animals.end(), [](const Animal& a) { return a.gone; }), animals.end());
}

void Game::hitAnimal(Animal& a, float dmg, V2 dir, bool crit) {
    int d = std::max(1, (int)roundf(dmg));
    a.hp -= d;
    a.flash = 0.16f;
    a.hpShow = 3.0f;
    a.kb += dir * (crit ? 150.f : 105.f);
    bloodBurst(a.pos + V2(0, -a.bodyH), dir, crit ? 14 : 8);
    ftext(a.pos + V2(0, -a.bodyH - 10), (crit ? std::string("CRIT ") : std::string()) + S(d), crit ? C_ORANGE : C_TEXT, 0.9f);
    audio.play(crit ? S_CRIT : S_HIT);
    shakeAmt = std::max(shakeAmt, crit ? 3.f : 1.5f);
    decals.push_back({a.pos + V2(rng.range(-5, 5), rng.range(0, 4)), 0, 0, 40});
    const AnimalDef& D = ANIMAL[a.kind];
    bool agg = D.aggro && (!(D.flags & BH_NIGHT_AGGRO) || daylight() < 0.55f);
    if (agg) {
        a.st = AS_CHASE;
        a.stT = 10;
    } else {
        a.st = AS_FLEE;
        a.stT = 7;
    }
    if (a.hp <= 0) killAnimal(a);
}

void Game::killAnimal(Animal& a) {
    a.dead = true;
    a.deadT = 0;
    a.hp = 0;
    a.vel = V2();
    kills++;
    questEvent(Q_KILL, a.kind, 1);
    const AnimalDef& D = ANIMAL[a.kind];
    int total = 0;
    for (int m = 0; m < M_COUNT; m++) {
        a.remain[m] = D.drop[m][1] > 0 ? rng.irange(D.drop[m][0], D.drop[m][1]) : 0;
        total += a.remain[m];
    }
    if (D.formDrop[0] >= 0 && a.variant == 1) {  // e.g. a buck's antlers
        int n = rng.irange(D.formDrop[1], D.formDrop[2]);
        a.remain[D.formDrop[0]] += n;
        total += n;
    }
    if (a.tier > 0) {  // better specimens carry more
        total = 0;
        for (int m = 0; m < M_COUNT; m++) {
            a.remain[m] = (int)ceilf(a.remain[m] * TIER[a.tier].drops);
            total += a.remain[m];
        }
        if (a.tier == TR_LEGENDARY && D.legendDrop[0] >= 0) {
            int n = frng.irange(D.legendDrop[1], D.legendDrop[2]);
            a.remain[D.legendDrop[0]] += n;
            total += n;
        }
        if (TIER[a.tier].bounty > 0) {
            p.coins += TIER[a.tier].bounty;
            ftext(a.pos + V2(0, -a.bodyH - 30), "+" + S(TIER[a.tier].bounty) + "C", C_GOLD, 1.6f);
        }
        msg(std::string(TIER[a.tier].name) + " " + animalName(a.kind, a.tier) + " HUNTED!", TIER[a.tier].col);
    }
    a.air = 0;
    if (total == 0) {
        a.remain[M_MEAT] = 1;
        total = 1;
    }
    a.totalRemain = total;
    decals.push_back({a.pos + V2(0, 3), D.decal, 0, 80});
    bloodBurst(a.pos + V2(0, -a.bodyH), a.kb.norm(), 16);
    audio.play(S_KILL);
    shakeAmt = std::max(shakeAmt, 4.f);
    ftext(a.pos + V2(0, -a.bodyH - 18), "HUNTED!", C_GOLD, 1.4f);
    // skill XP for a completed kill (every kill is made with a thrown weapon or a launcher); bigger and rarer game pays more
    const float tierMul = a.tier > 0 ? TIER[a.tier].drops : 1.f;
    const int rec = 5 + (int)(D.hp / 3);  // level at which this animal is no longer a challenge
    grantXp(SK_HUNTING, std::max(1, (int)lroundf((3 + D.hp / 6.f) * tierMul * xpFalloff(p.sk.level(SK_HUNTING), rec))));
    grantXp(SK_RANGED, std::max(1, (int)lroundf((2 + D.hp / 8.f) * tierMul * xpFalloff(p.sk.level(SK_RANGED), rec))));
}

// ================================================================= player
void Game::hurtPlayer(float dmg, V2 from, int srcKind) {
    if (p.invuln > 0 || p.dead) return;
    dmg *= 1.f - skillBonus(SK_DEFENSE);
    if (p.has(CG_VENOMWRAPS) && (srcKind == A_RATTLER || srcKind == A_MOCCASIN)) dmg *= 0.65f;  // snake-proof wraps
    int d = std::max(1, (int)ceilf(dmg * (1.f - ARMOR_RED[p.up[U_ARMOR]])));
    p.hp -= d;
    if (p.hp > 0 && p.up[U_ARMOR] > 0) grantXp(SK_DEFENSE, std::max(1, d / 8));  // surviving a hit in armor
    p.invuln = 0.85f;
    p.sinceHurt = 0;
    V2 dir = (p.pos - from).norm();
    p.kb = dir * 140;
    shakeAmt = 5.5f;
    audio.play(S_HURT);
    ftext(p.pos + V2(0, -24), "-" + S(d), C_RED, 1.0f);
    bloodBurst(p.pos + V2(0, -8), dir, 9);
    if (p.hp <= 0) {
        p.hp = 0;
        p.dead = true;
        p.deadT = 0;
        p.charging = false;
        int loss = (int)(p.coins * 0.15f);
        p.coins -= loss;
        msg("YOU WERE KNOCKED OUT!  LOST " + S(loss) + " COINS", C_RED);
    }
}

// A weapon can be used when you own it and (for the bow and crossbow) have ammo for it.
bool Game::canUse(int w) const { return p.wpn(w) > 0 && (!isLauncher(w) || p.ammo(WPN[w].ammo) > 0); }

void Game::throwWeapon() {
    int w = p.sel;
    if (!canUse(w)) return;
    const WpnDef& D = WPN[w];
    const bool launcher = isLauncher(w);
    if (launcher) p.inv.remove(itemAmmo(D.ammo), 1);  // launchers are not used up, their ammunition is
    else p.inv.remove(itemWpn(w), 1);
    V2 hand = p.pos + V2(0, -10);
    V2 mw(in.mx + camX, in.my + camY);
    V2 d = mw - hand;
    float dl = d.len();
    V2 dir = dl > 4 ? d * (1.f / dl) : p.aim;
    float c = p.charge;
    Proj pr;
    pr.pos = hand + dir * 8;
    pr.last = pr.pos;
    pr.dir = dir;
    pr.speed = D.speed * (1 + 0.15f * c);
    pr.range = std::min(D.range * (1 + 0.15f * c), dl + 24);
    pr.travelled = 0;
    pr.wpn = w;
    pr.angle = atan2f(dir.y, dir.x);
    const bool spearLike = w == W_JAVELIN || w == W_SPEAR || w == W_IRONSPEAR || w == W_RSPEAR || w == W_SUNJAV || w == W_BOGSPEAR || w == W_HARPOON;
    const bool axeLike = w == W_AXE || w == W_HVYAXE || w == W_CLIFFAXE;
    pr.spin = (spearLike || launcher) ? 0.f : (axeLike ? 17.f : 26.f);
    const float biomeMul = (D.biome >= 0 && curBiome == D.biome) ? D.biomeMul : 1.f;  // biome weapons: +30% at home
    pr.crit = rng.chance(0.10f);
    pr.dmg = D.dmg * biomeMul * (1.f + skillBonus(SK_HUNTING)) * (1 + 0.4f * c) * rng.range(0.9f, 1.1f) * (pr.crit ? 1.6f : 1.f);
    if (launcher) {
        // a bow shot is only as good as the draw; the crossbow is always fully loaded when it fires
        float draw = D.draw > 0 ? c : 1.f;
        pr.speed = D.speed * (0.55f + 0.45f * draw);
        pr.range = std::min(D.range * (0.5f + 0.5f * draw), dl + 24);
        pr.dmg = D.dmg * biomeMul * (1.f + skillBonus(SK_HUNTING)) * (0.45f + 0.55f * draw) * rng.range(0.92f, 1.08f) * (pr.crit ? 1.6f : 1.f);
        pr.ammo = D.ammo;
    }
    projs.push_back(pr);
    p.cd = D.cd;
    p.throwAnim = 0.3f;
    audio.play(S_THROW, 0.6f);
    if (!canUse(w) && !launcher) {  // ran out of thrown weapons: switch to another one you can use
        for (int i = 0; i < W_COUNT; i++)
            if (canUse(i)) {
                p.sel = i;
                break;
            }
    }
}

void Game::addItem(int m, int n) { p.inv.add(itemMat(m), n); }

void Game::gatherStep(Animal& c) {
    int m = -1;
    for (int i = 0; i < M_COUNT; i++) {
        int mm = GATHER_ORDER[i];
        if (c.remain[mm] > 0) {
            m = mm;
            break;
        }
    }
    if (m < 0) {
        c.totalRemain = 0;
        return;
    }
    if (!p.inv.canAdd(itemMat(m), 1)) {
        if (bagWarn <= 0) {
            ftext(p.pos + V2(0, -28), "BAG FULL!", C_RED, 1.2f);
            msg("BAG FULL - SELL AT THE SHOP", C_RED);
            audio.play(S_ERROR);
            bagWarn = 1.6f;
        }
        return;
    }
    c.remain[m]--;
    c.totalRemain--;
    p.inv.add(itemMat(m), 1);
    int extra = 0;
    const bool skinPiece = m == M_HIDE || m == M_FUR || m == M_SCALE || isTrophy(m);
    if (rng.chance(TOOL_BONUS[p.up[U_TOOL]] + skillBonus(skinPiece ? SK_SKINNING : SK_BUTCHERING)) && p.inv.canAdd(itemMat(m), 1)) {
        p.inv.add(itemMat(m), 1);
        extra = 1;
    }
    questEvent(Q_GATHER, m, 1 + extra);
    {  // butchering for meat, bone and fat; skinning for hide, fur, scales and the trophy pieces
        grantXp(skinPiece ? SK_SKINNING : SK_BUTCHERING, std::min(6, std::max(1, MAT_PRICE[m] / 5)) * (1 + extra));
    }
    ftext(c.pos + V2(rng.range(-6, 6), -10), "+" + S(1 + extra) + " " + MAT_NAME[m], extra ? C_GOLD : C_GREEN, 1.2f);
    bloodBurst(c.pos + V2(0, -4), V2(rng.range(-1, 1), -0.6f), 4);
    puff(c.pos + V2(0, -2), 3, HEX(0xd8c8a8));
    audio.play(S_GATHER);
    if (c.totalRemain <= 0) {
        ftext(c.pos + V2(0, -22), "CLEANED", C_TEXT, 1.0f);
        audio.play(S_PICKUP, 0.4f);
    }
}

void Game::updatePlayer(float dt) {
    Player& P = p;
    float newMax = (float)ARMOR_HP[P.up[U_ARMOR]];
    if (newMax > P.maxhp) P.hp += newMax - P.maxhp;
    P.maxhp = newMax;
    if (P.dead) {
        P.deadT += dt;
        if (P.deadT > 3.4f) {
            P.pos = world.spawn;
            P.hp = P.maxhp;
            P.dead = false;
            P.invuln = 2.f;
            P.kb = V2();
            for (auto& a : animals)
                if (!a.dead && a.st == AS_CHASE) {
                    a.st = AS_WANDER;
                    a.idleT = 2;
                }
            setCam(0, true);
        }
        return;
    }
    P.cd = std::max(0.f, P.cd - dt);
    P.invuln = std::max(0.f, P.invuln - dt);
    P.sinceHurt += dt;
    P.throwAnim = std::max(0.f, P.throwAnim - dt);

    if (in.selectKey >= 0 && in.selectKey < W_COUNT) P.sel = in.selectKey;
    if (in.wheel != 0) P.sel = ((P.sel + (in.wheel > 0 ? -1 : 1)) % W_COUNT + W_COUNT) % W_COUNT;

    V2 mw(in.mx + camX, in.my + camY);
    V2 hand = P.pos + V2(0, -10);
    V2 aimv = mw - hand;
    if (aimv.len() > 1) P.aim = aimv.norm();

    Animal* c = nearestCarcass(26);
    bool wantGather = in.e && c != nullptr && !P.charging;
    // E priority: a carcass, then a placed structure (chest / campfire), then a tree, plant or rock within reach
    const bool structBusy = !c && structInteract(dt);
    HTarget ht;
    if (!c && !structBusy) findHarvest(ht);
    bool harvestOk = false;
    if (ht.valid) {
        const ResDef& RD = RES[ht.res];
        harvestOk = RD.tool < 0 || bestTool(RD.tool) >= RD.minTier;
        if (!harvestOk && in.ePressed && hvMsgT <= 0) {
            msg(std::string("NEED A ") + TOOLDEF[RD.tool * 4 + RD.minTier - 1].name + " FOR " + RD.name, C_RED);
            audio.play(S_ERROR, 0.5f);
            hvMsgT = 1.5f;
        }
    }
    // being hit knocks you out of the swing; charging a throw pauses it (combat always wins)
    const bool wantHarvest = in.e && ht.valid && harvestOk && !P.charging && P.sinceHurt > 0.5f;

    V2 mv((in.right ? 1.f : 0.f) - (in.left ? 1.f : 0.f), (in.down ? 1.f : 0.f) - (in.up ? 1.f : 0.f));
    bool moving = mv.len() > 0.1f;
    if (moving) mv = mv.norm();
    float spd = 70.f * BOOT_MUL[P.up[U_BOOTS]] * (P.swiftT > 0 ? 1.15f : 1.f);
    const uint8_t groundT = world.tileAt(P.pos.x, P.pos.y);
    { float ts = tileSpeed(groundT); if (ts < 0.93f && p.has(CG_WADERS) && (curBiome == B_SWAMP)) ts = 0.93f; spd *= ts; }  // sand, mud, deep snow slow you down
    float drain = 1.f, regen = 1.f;
    climateMods(drain, regen);
    bool sprinting = false;
    if (in.sprint && moving && !P.exhausted && P.stamina > 0 && !wantGather && !wantHarvest) {
        sprinting = true;
        spd *= 1.55f;
        P.stamina -= 24 * dt * drain;
        P.staDelay = 0.7f;
        if (P.stamina <= 0) {
            P.stamina = 0;
            P.exhausted = true;
        }
    } else {
        P.staDelay -= dt;
        if (P.staDelay <= 0) P.stamina = std::min(100.f, P.stamina + (moving ? 16.f : 32.f) * dt * regen);
        if (P.exhausted && P.stamina > 30) P.exhausted = false;
    }
    if (P.charging) spd *= 0.6f;
    if (wantGather || wantHarvest) spd = 0;
    V2 want = mv * spd;
    if (groundT == T_ICE) {  // slippery: velocity eases towards the wanted velocity instead of snapping to it
        P.iceV = P.iceV + (want - P.iceV) * (1.f - expf(-2.4f * dt));
        want = P.iceV;
    } else
        P.iceV = want;
    V2 vel = want + P.kb;
    P.kb = P.kb * expf(-9 * dt);
    moveEnt(world, P.pos, vel * dt, 4.f);
    P.pos.x = clampf(P.pos.x, 20, world.pixW() - 20);
    P.pos.y = clampf(P.pos.y, 20, world.pixH() - 20);

    auto setDir = [&](V2 d) {
        if (fabsf(d.x) > fabsf(d.y) * 0.9f) {
            P.dir = 2;
            P.flip = d.x < 0;
        } else
            P.dir = d.y > 0 ? 0 : 1;
    };
    if (wantGather) setDir(c->pos - P.pos);
    else if (wantHarvest) setDir(ht.pos - P.pos);
    else if (P.charging || P.throwAnim > 0) setDir(P.aim);
    else if (moving) setDir(mv);
    if (moving && !wantGather && !wantHarvest) P.animT += dt * (sprinting ? 12.f : 8.5f);
    if (sprinting && rng.chance(0.25f)) puff(P.pos + V2(0, -1), 1, HEX(0xcdb98f));

    // --- throwing
    if (in.lPressed && !wantGather && P.cd <= 0) {
        if (canUse(P.sel)) {
            const WpnDef& SW = WPN[P.sel];
            if (isLauncher(P.sel) && SW.draw <= 0.f) {  // crossbow: loaded, so it fires at once and then needs reloading
                P.charge = 1.f;
                throwWeapon();
                P.charge = 0;
            } else {
                P.charging = true;
                P.charge = 0;
            }
        } else if (P.wpn(P.sel) > 0 && isLauncher(P.sel) && noAmmoT <= 0) {
            ftext(P.pos + V2(0, -24), std::string("NO ") + AMMO[WPN[P.sel].ammo].name + "S", C_RED, 1.0f);
            audio.play(S_ERROR, 0.5f);
            noAmmoT = 0.8f;
        }
    }
    if (noAmmoT > 0) noAmmoT -= dt;
    if (P.charging) {
        if (!canUse(P.sel)) P.charging = false;
        else {
            const float drawT = WPN[P.sel].draw > 0 ? WPN[P.sel].draw : 0.5f;
            P.charge = std::min(1.f, P.charge + dt / drawT);
            if (in.lReleased || !in.l) {
                throwWeapon();
                P.charging = false;
                P.charge = 0;
            }
        }
    }

    // --- pick thrown weapons back up
    for (size_t i = 0; i < items.size();) {
        GroundItem& it = items[i];
        if (it.ammo >= 0) {
            if (dist(P.pos, it.pos) < 13 && P.inv.canAdd(itemAmmo(it.ammo), 1)) {
                P.inv.add(itemAmmo(it.ammo), 1);
                ftext(P.pos + V2(0, -24), std::string("+1 ") + AMMO[it.ammo].name, C_BLUE, 0.9f);
                audio.play(S_PICKUP, 0.4f);
                items.erase(items.begin() + i);
            } else
                i++;
            continue;
        }
        if (dist(P.pos, it.pos) < 13 && !P.inv.canAdd(itemWpn(it.wpn), 1) && bagWarn <= 0) {
            ftext(P.pos + V2(0, -24), "BAG FULL!", C_RED, 1.0f);
            bagWarn = 1.6f;
        }
        if (dist(P.pos, it.pos) < 13 && P.inv.canAdd(itemWpn(it.wpn), 1)) {
            P.inv.add(itemWpn(it.wpn), 1);
            ftext(P.pos + V2(0, -24), std::string("+") + WPN[it.wpn].name, C_BLUE, 1.0f);
            audio.play(S_PICKUP, 0.5f);
            items.erase(items.begin() + i);
        } else
            i++;
    }

    // --- gathering
    if (wantGather) {
        P.gathering = true;
        float need = ANIMAL[c->kind].gatherTime;
        P.gatherT += dt * TOOL_SPEED[P.up[U_TOOL]];
        P.gatherSfx -= dt;
        if (P.gatherSfx <= 0) {
            P.gatherSfx = 0.17f / TOOL_SPEED[P.up[U_TOOL]];
            puff(c->pos + V2(rng.range(-5, 5), -3), 1, HEX(0xd8c8a8));
        }
        if (P.gatherT >= need) {
            P.gatherT = 0;
            gatherStep(*c);
        }
    } else if (wantHarvest) {
        harvestStep(ht, dt);
    } else {
        P.gathering = false;
        P.gatherT = std::max(0.f, P.gatherT - dt * 2.f);
    }

    // --- shop / bandage / regen
    if (in.ePressed && !c && !structBusy && !ht.valid) {
        if (nearShop()) {
            state = ST_SHOP;
            audio.play(S_CLICK);
        } else if (nearQuestGiver()) {
            state = ST_QUEST;
            audio.play(S_CLICK);
        }
    }
    if (in.bandage && P.hp < P.maxhp) {  // Q: the lightest healing item you carry first, strong medicine last
        static const int order[] = {C_BANDAGE, C_SALVE, C_JERKY, C_COOKED, C_POULTICE, C_TONIC, C_STEW, C_MEDICINE};
        for (int cid : order)
            if (P.inv.count(itemCons(cid)) > 0) {
                useConsumable(cid);
                break;
            }
    }
    if (P.sinceHurt > 6.f) P.hp = std::min(P.maxhp, P.hp + (nearShop() ? 12.f : 1.6f) * dt);
}

void Game::updateProjectiles(float dt) {
    for (size_t i = 0; i < projs.size();) {
        Proj& pr = projs[i];
        bool done = false;
        float step = pr.speed * dt;
        int sub = std::max(1, (int)ceilf(step / 3.f));
        float s = step / sub;
        for (int k = 0; k < sub && !done; k++) {
            pr.last = pr.pos;
            pr.pos += pr.dir * s;
            pr.travelled += s;
            for (auto& a : animals) {
                if (a.dead) continue;
                V2 c = a.pos + V2(0, -a.bodyH - a.air);
                if (dist(pr.pos, c) < ANIMAL[a.kind].hitR * a.scale + 3) {
                    hitAnimal(a, pr.dmg, pr.dir, pr.crit);
                    if (rng.chance((pr.ammo >= 0 ? AMMO[pr.ammo].lost : WPN[pr.wpn].brk) * (pr.ammo >= 0 && p.has(CG_QUIVER) ? 0.5f : 1.f) * (pr.ammo >= 0 ? 1.f - skillBonus(SK_RANGED) : 1.f))) {
                        for (int q = 0; q < 6; q++)
                            parts.push_back({pr.pos.x, pr.pos.y, 4, rng.range(-50, 50), rng.range(-30, 30), rng.range(30, 80), 0.5f, 0.5f, HEX(0xd8dce4), 1, true});
                        audio.play(S_BREAK, 0.6f);
                        ftext(a.pos + V2(0, -a.bodyH - 22), "SNAP", C_DIM, 0.7f);
                    } else {
                        items.push_back({a.pos + V2(rng.range(-9, 9), rng.range(0, 7)), pr.wpn, rng.range(0, 6.28f), 0, pr.ammo});
                    }
                    done = true;
                    break;
                }
            }
            if (done) break;
            V2 g = pr.pos + V2(0, 9);
            int tx = (int)floorf(g.x / TS), ty = (int)floorf(g.y / TS);
            bool obstacle = tx < 0 || ty < 0 || tx >= MW || ty >= MH || world.solid[ty][tx] || world.tile[ty][tx] == T_CLIFF;
            if (obstacle) {
                V2 lg = pr.last + V2(0, 9);
                items.push_back({lg, pr.wpn, pr.angle + pr.spin * (pr.travelled / pr.speed), 0, pr.ammo});
                for (int q = 0; q < 4; q++)
                    parts.push_back({pr.pos.x, pr.pos.y, 3, rng.range(-40, 40), rng.range(-20, 20), rng.range(20, 60), 0.4f, 0.4f, HEX(0xffe9a8), 1, true});
                audio.play(S_CLICK, 0.5f);
                done = true;
            } else if (pr.travelled >= pr.range) {
                if (world.water(tx, ty)) {
                    for (int q = 0; q < 8; q++)
                        parts.push_back({g.x, g.y, 1, rng.range(-30, 30), rng.range(-15, 15), rng.range(30, 70), 0.6f, 0.6f, HEX(0xcfe9f5), 1, true});
                    ftext(g + V2(0, -8), "SPLASH", C_BLUE, 0.8f);
                } else {
                    float ang = pr.angle + pr.spin * (pr.travelled / pr.speed);
                    items.push_back({g, pr.wpn, ang, 0, pr.ammo});
                    puff(g, 3, HEX(0xcdb98f));
                }
                done = true;
            }
        }
        if (done) projs.erase(projs.begin() + i);
        else i++;
    }
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
            if (p.has(CG_PARKA)) exposed = 0.f;
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
        if (items[i].t > 300) items.erase(items.begin() + i);
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
        if (canUse(w)) return;                // still holding a weapon (a bow with no arrows doesn't count)
    if (!projs.empty() || !items.empty()) return;  // a thrown weapon can still be picked up
    int wealth = p.coins;
    for (int m = 0; m < M_COUNT; m++) wealth += p.mat(m) * MAT_PRICE[m];
    if (wealth >= FLINT_PACK_PRICE) return;  // could buy flint at the shop
    if (!p.inv.canAdd(itemWpn(W_FLINT), FLINT_LOAN)) {  // bag stuffed with junk: make room by dropping the cheapest material stack
        int cheapest = -1;
        for (int m = 0; m < M_COUNT; m++)
            if (p.mat(m) > 0 && (cheapest < 0 || MAT_PRICE[m] < MAT_PRICE[cheapest])) cheapest = m;
        if (cheapest >= 0) p.inv.remove(itemMat(cheapest), p.mat(cheapest));
    }
    p.inv.add(itemWpn(W_FLINT), FLINT_LOAN);
    p.sel = W_FLINT;
    msg("THE MERCHANT LENDS YOU " + S(FLINT_LOAN) + " FLINT KNIVES", C_GOLD);
    ftext(p.pos + V2(0, -28), "+" + S(FLINT_LOAN) + " FLINT KNIFE", C_BLUE, 1.4f);
    audio.play(S_PICKUP);
}

void Game::updatePlay(float dt) {
    dayT += dt;
    if (helpT > 0) helpT -= dt;
    if (bagWarn > 0) bagWarn -= dt;
    updateBiome(dt);
    updatePlayer(dt);
    updateHarvest(dt);
    updateStructures(dt);
    p.warmT = std::max(0.f, p.warmT - dt);
    p.coolT = std::max(0.f, p.coolT - dt);
    p.swiftT = std::max(0.f, p.swiftT - dt);
    p.nightT = std::max(0.f, p.nightT - dt);
    updateAnimals(dt);
    updateProjectiles(dt);
    checkWeaponSafetyNet(dt);
    updateFx(dt);
    updateWeather(dt);
    setCam(dt);
    // keep the wilds populated
    respawnT -= dt;
    if (respawnT <= 0 && world.biomeAtPos(p.pos.x, p.pos.y) != B_HOME) respawnT = 4.f;  // home herds only refill while you are home
    if (respawnT <= 0) {
        respawnT = 4.f;
        int alive[A_COUNT] = {0};
        for (auto& a : animals)
            if (!a.dead) alive[a.kind]++;
        int targets[4] = {14, 8, 5, 4 + (int)(2 * (1.f - daylight()))};
        for (int k = 0; k < 4; k++)  // the original four species keep their home-area ring population
            if (alive[k] < targets[k]) trySpawn(k, 230, true);
    }
    streamSpawns(dt);
    checkPortals(dt);  // last: a trip replaces the world, so nothing above may run on it afterwards
}

void Game::update(float dt) {
    timeAcc += dt;
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
                newGame();
                state = ST_PLAY;
                audio.play(S_CLICK);
            }
            break;
        }
        case ST_PLAY:
            if (in.escPressed) {
                state = ST_PAUSE;
                break;
            }
            if (in.mapToggle) showMap = !showMap;
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
        case ST_SHOP:
            dayT += 0;  // time stands still while trading
            updateFx(dt);
            if (in.escPressed || in.ePressed) {
                state = ST_PLAY;
                save();
                audio.play(S_CLICK);
            }
            if (in.tabPressed) shopTab = (shopTab + 1) % 4;
            if (shopTab == 0) sellScroll -= in.wheel;
            if (in.selectKey >= 0 && in.selectKey < 4) shopTab = in.selectKey;
            break;
        case ST_CHEST:
            updateChest(dt);
            break;
        case ST_PAUSE:
            if (in.escPressed) state = ST_PLAY;
            break;
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

// ================================================================= inventory screen
constexpr int INV_COLS = 8;

// Eat / drink / apply one consumable (used from the pack and by the Q quick-heal key).
bool Game::useConsumable(int c) {
    const int id = itemCons(c);
    if (p.inv.count(id) <= 0) return false;
    auto heal = [&](int amount) {
        if (p.hp >= p.maxhp) {
            msg("YOU ARE AT FULL HEALTH", C_DIM);
            audio.play(S_ERROR);
            return false;
        }
        amount = (int)lroundf(amount * (1.f + skillBonus(SK_SURVIVAL)));
        p.hp = std::min(p.maxhp, p.hp + amount);
        ftext(p.pos + V2(0, -24), "+" + S(amount) + " HP", C_GREEN, 1.1f);
        return true;
    };
    switch (c) {
        case C_BANDAGE: if (!heal(BANDAGE_HEAL)) return false; break;
        case C_SALVE: if (!heal(30)) return false; break;
        case C_TONIC: if (!heal(90)) return false; break;
        case C_COOKED:
            if (!heal(35)) return false;
            p.stamina = 100.f;
            p.exhausted = false;
            break;
        case C_BREW:
            if (p.warmT > 60.f) {
                msg("YOU ARE ALREADY WARM", C_DIM);
                audio.play(S_ERROR);
                return false;
            }
            p.warmT = 180.f;
            ftext(p.pos + V2(0, -24), "WARM!", C_ORANGE, 1.2f);
            break;
        case C_CACTUSWATER:
            p.stamina = 100.f;
            p.exhausted = false;
            p.coolT = 120.f;
            ftext(p.pos + V2(0, -24), "REFRESHED!", C_BLUE, 1.2f);
            break;
        case C_POULTICE: if (!heal(60)) return false; break;
        case C_MEDICINE: if (!heal(120)) return false; break;
        case C_JERKY:
            if (!heal(20)) return false;
            p.stamina = 100.f;
            p.exhausted = false;
            break;
        case C_STEW:
            if (!heal(85)) return false;
            p.stamina = 100.f;
            p.exhausted = false;
            break;
        case C_VIGOR:
            if (p.hp >= p.maxhp && p.swiftT > 30.f) {
                msg("YOU ARE ALREADY INVIGORATED", C_DIM);
                audio.play(S_ERROR);
                return false;
            }
            p.hp = std::min(p.maxhp, p.hp + 55.f);
            p.stamina = 100.f;
            p.exhausted = false;
            p.swiftT = 90.f;
            ftext(p.pos + V2(0, -24), "INVIGORATED!", C_GREEN, 1.2f);
            break;
        case C_NIGHTSIGHT:
            if (p.nightT > 120.f) {
                msg("YOUR EYES ARE ALREADY SHARP", C_DIM);
                audio.play(S_ERROR);
                return false;
            }
            p.nightT = 300.f;
            ftext(p.pos + V2(0, -24), "NIGHTSIGHT!", C_BLUE, 1.2f);
            break;
        default: return false;
    }
    p.inv.remove(id, 1);
    audio.play(S_PICKUP);
    grantXp(SK_SURVIVAL, 2);  // only reached when the item really took effect (refusals return earlier)
    return true;
}

// Equip a weapon or use a consumable from the pack. Returns true when something happened.
bool Game::useSlot(int slot) {
    if (slot < 0 || slot >= p.bagCap() || p.inv.slots[slot].empty()) return false;
    const int id = p.inv.slots[slot].item;
    const ItemDef& D = item(id);
    switch (D.kind) {
        case IK_THROWN:
        case IK_LAUNCHER:
            p.sel = D.ref;
            p.charging = false;
            msg("EQUIPPED " + D.name, C_GOLD);
            audio.play(S_CLICK);
            return true;
        case IK_AMMO:
            for (int w = 0; w < W_COUNT; w++)  // ammunition equips the launcher that fires it
                if (WPN[w].ammo == D.ref && p.wpn(w) > 0) {
                    p.sel = w;
                    msg("EQUIPPED " + std::string(WPN[w].name), C_GOLD);
                    audio.play(S_CLICK);
                    return true;
                }
            msg("YOU HAVE NOTHING THAT FIRES " + D.name + "S", C_RED);
            audio.play(S_ERROR);
            return false;
        case IK_CONSUMABLE: return useConsumable(D.ref);
        case IK_TOOL:
            msg("TOOLS WORK AUTOMATICALLY - THE BEST ONE IN YOUR PACK IS USED", C_DIM);
            return false;
        case IK_PLACEABLE:
            if (placeStructure(D.ref)) {
                state = ST_PLAY;  // close the pack so the new structure is visible
                return true;
            }
            return false;
        default:
            msg(D.name + " - SELL IT AT THE SHOP", C_DIM);
            return false;
    }
}

void Game::updateInventory(float dt) {
    updateFx(dt);  // time stands still while the pack is open
    if (in.escPressed || in.invToggle) {
        state = ST_PLAY;
        save();
        audio.play(S_CLICK);
        return;
    }
    if (in.tabPressed) invTab = (invTab + 1) % 3;
    // cursor: arrow keys / WASD, one step per key press
    const bool now[4] = {in.up, in.down, in.left, in.right};
    const int cap = p.bagCap();
    if (invTab == 0) {
        int dx = (now[3] && !navPrev[3]) - (now[2] && !navPrev[2]), dy = (now[1] && !navPrev[1]) - (now[0] && !navPrev[0]);
        if (dx || dy) {
            int c = invSel % INV_COLS + dx, r = invSel / INV_COLS + dy;
            int rows = (cap + INV_COLS - 1) / INV_COLS;
            c = std::max(0, std::min(INV_COLS - 1, c));
            r = std::max(0, std::min(rows - 1, r));
            invSel = std::min(cap - 1, r * INV_COLS + c);
        }
        if (in.enterPressed) useSlot(invSel);
    }
    if (invTab == 1) {  // crafting list: arrows and the wheel scroll it
        craftScroll += (now[1] && !navPrev[1]) - (now[0] && !navPrev[0]);
        craftScroll -= in.wheel;
    }
    for (int i = 0; i < 4; i++) navPrev[i] = now[i];
}

// ================================================================= economy
bool Game::canAfford(int coins, int m1, int n1, int m2, int n2) const {
    if (p.coins < coins) return false;
    if (m1 >= 0 && p.mat(m1) < n1) return false;
    if (m2 >= 0 && p.mat(m2) < n2) return false;
    return true;
}
void Game::spend(int coins, int m1, int n1, int m2, int n2) {
    p.coins -= coins;
    if (m1 >= 0) p.inv.remove(itemMat(m1), n1);
    if (m2 >= 0) p.inv.remove(itemMat(m2), n2);
}
bool Game::buyUpgrade(int u) {
    int lv = p.up[u];
    if (lv >= 2) return false;
    const UpLevel& L = UPS[u].lv[lv];
    if (!canAfford(L.coins, L.m1, L.n1, L.m2, L.n2)) {
        audio.play(S_ERROR);
        return false;
    }
    spend(L.coins, L.m1, L.n1, L.m2, L.n2);
    p.up[u]++;
    if (u == U_BAG) p.syncPack();  // a bigger backpack only adds slots
    audio.play(S_CRAFT);
    msg(std::string("UPGRADED: ") + L.name, C_GOLD);
    save();
    return true;
}
void Game::sellMat(int m, int n) {
    n = std::min(n, p.mat(m));
    if (n <= 0) return;
    p.inv.remove(itemMat(m), n);
    p.coins += n * MAT_PRICE[m];
    sold += n * MAT_PRICE[m];
    audio.play(S_COIN);
    questEvent(Q_EARN, 0, n * MAT_PRICE[m]);
}

// ================================================================= quests
bool Game::nearQuestGiver() const { return dist(p.pos, world.questGiver) < 34; }

// Re-read the current quest from the data table (call whenever questIdx changes).
void Game::refreshQuest() {
    questIdx = std::max(0, questIdx);
    quest = questAt(questIdx);
    questProg = questActive ? std::min(std::max(0, questProg), quest.goal) : 0;
}

// Something happened in the world (a kill, gathered material, a craft, coins earned from selling).
// If it matches the active quest, count it.
void Game::questEvent(int type, int id, int n) {
    if (!questActive || questReady() || quest.type != type) return;
    if (quest.target >= 0 && quest.target != id) return;
    questProg = std::min(quest.goal, questProg + n);
    if (questReady()) {
        msg("QUEST DONE: " + quest.title + " - SEE THE ELDER", C_GREEN);
        ftext(p.pos + V2(0, -30), "QUEST DONE!", C_GREEN, 1.6f);
        audio.play(S_PICKUP);
    }
}

void Game::acceptQuest() {
    if (questActive) return;
    questActive = true;
    questProg = 0;
    msg("QUEST ACCEPTED: " + quest.title, C_GOLD);
    audio.play(S_PICKUP);
    save();
}

void Game::turnInQuest() {
    if (!questReady()) return;
    p.coins += quest.coins;
    std::string reward = S(quest.coins) + " COINS";
    if (quest.mat >= 0 && quest.matN > 0) {
        // materials only fit if the bag has room; anything left over is paid in coins instead
        int give = p.inv.add(itemMat(quest.mat), quest.matN);
        if (give < quest.matN) p.coins += (quest.matN - give) * MAT_PRICE[quest.mat];
        reward += " + " + S(quest.matN) + " " + MAT_NAME[quest.mat];
    }
    if (quest.bandages > 0) {
        p.inv.add(ITEM_CONS0 + C_BANDAGE, quest.bandages);
        reward += " + " + S(quest.bandages) + " BANDAGE";
    }
    msg("QUEST COMPLETE! " + reward, C_GOLD);
    ftext(p.pos + V2(0, -30), "+" + S(quest.coins) + " COINS", C_GOLD, 1.6f);
    audio.play(S_CRAFT);
    questIdx++;
    questActive = false;
    questProg = 0;
    refreshQuest();
    save();
}
