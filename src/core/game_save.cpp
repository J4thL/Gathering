// game_save.cpp - saving and loading (save format version 2, with recovery from a damaged file).
#include <cstdio>
#include <fstream>
#include <sstream>
#include "core/game.hpp"
#include "core/logic_util.hpp"

std::string Game::savePath() {
    // WILDWOOD_SAVE_DIR (set by main() for every test / tool run) keeps those runs away from the player's real save
    if (const char* d = getenv("WILDWOOD_SAVE_DIR"))
        if (*d) return std::string(d) + "/save.txt";
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
        // equipment: the worn bag and the hotbar (item keys; "-" = empty). Weights and the carrying limit are never saved - they are
        // computed from the items and the bag, so changing a weight in items.hpp applies to old saves too.
        f << "look " << p.look.hair << ' ' << p.look.color << ' ' << p.look.skin << ' ' << p.look.outfit << "\n";
        f << "bag " << (p.bag >= 0 ? itemKey(itemPack(p.bag)) : std::string("-")) << "\nfarmtools";
        for (int t : {(int)TT_HOE, (int)TT_CAN, (int)TT_SICKLE}) f << ' ' << (p.tool[t] >= 0 ? itemKey(itemTool(p.tool[t])) : std::string("-"));
        f << "\nhotbar";
        for (int i = 0; i < HOT_N; i++) f << ' ' << (p.hot[i] >= 0 ? itemKey(p.hot[i]) : std::string("-"));
        f << "\n";
        for (auto& sl : p.inv.slots)
            if (!sl.empty()) {  // (the quality is appended only above Normal: older builds read such a line as a plain stack)
                f << "slot " << itemKey(sl.item) << ' ' << sl.n;
                if (sl.q > 0) f << ' ' << sl.q;
                f << "\n";
            }
        f << "wornq " << p.armorQ << ' ' << p.bootsQ << ' ' << p.bagQ << "\n";  // quality of the worn armor, boots and bag
        // maps: "at <map>" is where the hunter is; each visited map then has a "map <id>" block with its discovered landmarks
        // ("disc"), the resources waiting to regrow ("hv") and its campfires / chests ("struct", "cslot"). Extra lines that older
        // builds simply ignore, so the save version stays 2.
        snapshotMap();  // (does nothing indoors: the outside map was parked in mapMem when the hunter went in)
        f << "at " << (interior >= 0 || delve >= 0 ? extMap : world.mapId) << "\n";
        f << delveSaveLines();
        f << hsSaveLines();
        if (delve >= 0) f << "atpos " << (int)extPos.x << ' ' << (int)extPos.y << "\n";  // saved inside a delve: the next load stands at its entrance
        if (interior >= 0) f << "atpos " << (int)extPos.x << ' ' << (int)extPos.y << "\n";  // saved inside a building: the next load stands at its door
        for (int m = 0; m < B_COUNT; m++) {
            const MapMem& mm = mapMem[m];
            if (!mm.visited) continue;
            f << "map " << m << "\ndisc " << mm.disc << "\n";
            if (!mm.seen.empty()) f << "seen " << mm.seen << "\n";
            for (auto& ln : mm.hv) f << ln << "\n";
            for (auto& ln : mm.st) f << ln << "\n";
            for (auto& g : mm.items)  // stacks the hunter dropped lie where they were left
                if (g.item >= 0 && g.n > 0) f << "drop " << itemKey(g.item) << ' ' << g.n << ' ' << (int)g.pos.x << ' ' << (int)g.pos.y << (g.q > 0 ? " " + std::to_string(g.q) : std::string()) << "\n";
        }
        f.flush();
        if (!f) {
            std::remove(tmp.c_str());
            return;
        }
    }
    // Replace the old save without a moment where no save exists: the old file becomes save.txt.bak, the new one takes its place, and
    // if that last step fails the old file is put back. (A plain remove-then-rename loses the save if anything goes wrong in between;
    // std::rename cannot overwrite an existing file on Windows, which is why the old file is moved aside rather than overwritten.)
    const std::string bak = path + ".bak";
    std::remove(bak.c_str());
    const bool hadOld = std::rename(path.c_str(), bak.c_str()) == 0;
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        if (hadOld) std::rename(bak.c_str(), path.c_str());
        std::remove(tmp.c_str());
        return;
    }
    hasSave = true;
}

// If the main save is missing but the previous one (save.txt.bak) is there - the game stopped between the two renames in save() - put it back.
void recoverSave(const std::string& path) {
    if (std::ifstream(path)) return;
    const std::string bak = path + ".bak";
    if (std::ifstream(bak)) std::rename(bak.c_str(), path.c_str());
}

bool Game::load() {
    villagers.clear();
    villagersInit = false;
    recoverSave(savePath());
    std::ifstream f(savePath());
    if (!f) return false;
    newGame();
    loadReport = LoadReport();
    auto readCount = [](std::istringstream& ss, long lim = 99999) {
        long v = 0;
        if (!(ss >> v)) return 0L;
        return std::max(0L, std::min(lim, v));
    };
    std::string bagKey = "-", hotKeys[HOT_N];
    bool sawBag = false, sawHotbar = false, sawFarm = false;
    std::vector<std::string> farmKeys;
    struct DropLine { std::string key; int n, x, y, q; };
    std::vector<DropLine> dropLines[B_COUNT];
    struct SlotLine {
        std::string key;
        int n, q;
    };
    std::vector<SlotLine> slotLines;  // v2
    // per-map state read from the file: "map <id>" starts a block; lines before any block come from a save made before the
    // separate maps existed (one big world) and are converted below
    std::vector<std::string> mapHv[B_COUNT], mapSt[B_COUNT];
    std::string mapDisc[B_COUNT], mapSeen[B_COUNT];
    std::vector<std::string> legHv, legSt;
    std::string legDisc;
    std::vector<std::pair<std::string, int>> legacyRefund;  // items from old out-of-home chests (see below)
    HunterLook lookLoaded;
    int curLoad = -1, atMap = B_HOME, atPosX = -1, atPosY = -1;
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
            const int n = (int)readCount(ss);
            int q = 0;
            if (!(ss >> q)) q = 0;  // (older saves: no quality field)
            slotLines.push_back({key, n, clampQuality(q)});
        } else if (k == "wornq") {
            ss >> p.armorQ >> p.bootsQ >> p.bagQ;
            if (!ss) p.armorQ = p.bootsQ = p.bagQ = 0;
            p.armorQ = clampQuality(p.armorQ);
            p.bootsQ = clampQuality(p.bootsQ);
            p.bagQ = clampQuality(p.bagQ);
        } else if (k == "look") {
            ss >> lookLoaded.hair >> lookLoaded.color >> lookLoaded.skin >> lookLoaded.outfit;
            if (!ss) lookLoaded = HunterLook();
        } else if (k == "bag") {
            ss >> bagKey;
            sawBag = true;
        } else if (k == "farmtools") {
            sawFarm = true;
            std::string key;
            while (ss >> key) farmKeys.push_back(key);
        } else if (k == "hotbar") {
            sawHotbar = true;
            for (int i = 0; i < HOT_N; i++) {
                std::string key;
                if (!(ss >> key)) break;
                hotKeys[i] = key;
            }
        } else if (k == "drop") {
            std::string key;
            int n = 0, x = 0, y = 0, q = 0;
            ss >> key >> n >> x >> y;
            if (ss && !(ss >> q)) q = 0;  // the quality is optional (older saves have none)
            if (curLoad >= 0 && n > 0) dropLines[curLoad].push_back({key, n, x, y, clampQuality(q)});
        } else if (k == "map") {
            ss >> curLoad;
            if (!ss || curLoad < 0 || curLoad >= B_COUNT) curLoad = -2;  // an unknown map: its lines are skipped
            sawMapBlock = true;
        } else if (k == "delve") {  // "delve <id> <opened chests mask> <king dead> <seconds to his return> <seconds to chest refill>"
            int id = -1, bd = 0, bt = 0, ct = 0;
            unsigned long mask = 0;
            ss >> id >> mask >> bd >> bt >> ct;
            if (ss && id >= 0 && id < DV_COUNT) {
                delveMem[id].visited = true;
                delveMem[id].chests = (uint32_t)mask;
                delveMem[id].bossDead = bd != 0;
                delveMem[id].bossTimer = (float)bt;
                delveMem[id].chestT = (float)ct;
            }
        } else if (k == "dhv") {  // "dhv <id> hv n x y res t": a harvested resource of that level
            int id = -1;
            ss >> id;
            std::string rest;
            std::getline(ss, rest);
            if (ss.fail() && rest.empty()) continue;
            if (id >= 0 && id < DV_COUNT) {
                delveMem[id].visited = true;
                delveMem[id].hv.push_back(rest.size() && rest[0] == ' ' ? rest.substr(1) : rest);
            }
        } else if (k == "hs" || k == "hsp" || k == "hsi" || k == "hsa") {
            hsLoadLine(k, ss);
        } else if (k == "atpos") {
            ss >> atPosX >> atPosY;
            if (!ss) atPosX = atPosY = -1;
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
        else if (k == "day") {  // a hand-edited or damaged value (negative, huge) must not break the clock and the day counter
            float d = 0.f;
            if (ss >> d && std::isfinite(d)) dayT = std::min(1.0e8f, std::max(0.f, d));
        } else if (k == "kills") {
            long long n = 0;
            if (ss >> n) kills = (int)std::max(0LL, std::min(1000000000LL, n));
        }
        else if (k == "skill") skillLoadLine(ss);
        else if (k == "disc") {
            std::string bits;
            ss >> bits;
            if (curLoad >= 0) mapDisc[curLoad] = bits;
            else if (curLoad == -1) legDisc = bits;
        } else if (k == "seen") {  // fog of war of the current map block (absent in older saves: the map starts unexplored)
            std::string bits;
            ss >> bits;
            if (curLoad >= 0 && bits.size() == (size_t)MM_W * MM_H) mapSeen[curLoad] = bits;
        } else if (k == "quest") {  // "quest <index> <active 0/1> <progress>"; older saves simply lack this line
            int active = 0;
            ss >> questIdx >> active >> questProg;
            questIdx = std::max(0, std::min(1000000, questIdx));  // (0 when the numbers are unreadable)
            questActive = active != 0;
        }
    }
    if (!sawVersion && !sawLegacy) {  // not a save file we understand: say so instead of wiping the hunter's kit
        loadReport.version = 0;
        newGame();
        return false;
    }
    for (int i = 0; i < U_COUNT; i++) p.up[i] = std::min(UP_MAX[i], std::max(0, p.up[i]));  // the top levels (scale mail, reinforced leather, bear hide, swiftfoot boots, explorer pack) are craft-only
    // The pack always has 100 slots; the worn bag only sets the carrying limit. A save from before bags were items stored a backpack
    // upgrade level (1-3 = satchel, trapper, explorer): it becomes the worn bag. Items are loaded WITHOUT the weight limit - an old
    // pack may be heavier than the new limits allow, and nothing a hunter owns may be lost (they just cannot pick up more until lighter).
    p.bag = -1;
    if (sawBag) {
        const int id = itemFromKey(bagKey);
        if (id >= ITEM_PACK0 && id < ITEM_PACK0 + PK_COUNT) p.bag = id - ITEM_PACK0;
    } else if (p.up[U_BAG] >= 1)
        p.bag = std::min(p.up[U_BAG], (int)PK_EXPLORER + 1) - 1;
    p.up[U_BAG] = 0;
    p.syncPack();
    p.inv.clear();
    auto give = [&](int item, int n, int q = 0) {
        if (n <= 0) return;
        int got = p.inv.addIgnoringWeight(item, n, q);
        loadReport.dropped += n - got;
    };
    if (sawVersion) {
        for (auto& sl : slotLines) {
            int id = itemFromKey(sl.key);
            if (id < 0) {
                loadReport.unknown++;  // an item from a newer game version: skipped, the rest still loads
                continue;
            }
            give(id, sl.n, sl.q);
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
    // Farming tools: the equipped ones are saved by key. An older save has no such line, so the hoe / can / sickle it carried are equipped
    // (they used to work from the pack), which keeps every old save's farm working.
    for (int t = 0; t < TT_COUNT; t++) p.tool[t] = -1;
    if (sawFarm) {
        for (auto& key : farmKeys) {
            const int id = itemFromKey(key);
            if (id >= 0 && item(id).kind == IK_TOOL && isFarmType(TOOLDEF[item(id).ref].type)) p.tool[TOOLDEF[item(id).ref].type] = item(id).ref;
        }
    } else
        for (int k = 0; k < TL_COUNT; k++)
            if (isFarmType(TOOLDEF[k].type) && p.tool[TOOLDEF[k].type] < 0 && p.inv.count(itemTool(k)) > 0) {
                p.tool[TOOLDEF[k].type] = k;
            }
    applyLook(lookLoaded);
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
        if (id >= 0) loadReport.dropped += rf.second - p.inv.addIgnoringWeight(id, rf.second);
    }
    for (int m = 0; m < B_COUNT; m++) {
        mapMem[m].hv = mapHv[m];
        mapMem[m].st = mapSt[m];
        mapMem[m].disc = mapDisc[m];
        mapMem[m].seen = mapSeen[m];
        mapMem[m].visited = !mapHv[m].empty() || !mapSt[m].empty() || !mapDisc[m].empty() || m == atMap;
        mapMem[m].items.clear();
        for (auto& d : dropLines[m]) {
            const int id = itemFromKey(d.key);
            if (id < 0) continue;
            GroundItem g{};
            g.pos = V2((float)d.x, (float)d.y);
            g.item = id;
            g.n = d.n;
            g.q = d.q;
            g.ammo = -1;
            mapMem[m].items.push_back(g);
            mapMem[m].visited = true;
        }
    }
    // hotbar: from the save; a save from before the hotbar existed gets what it owns equipped once (the strongest weapons first)
    for (int i = 0; i < HOT_N; i++) p.hot[i] = -1;
    if (sawHotbar) {
        for (int i = 0; i < HOT_N; i++) {
            const int id = hotKeys[i].empty() || hotKeys[i] == "-" ? -1 : itemFromKey(hotKeys[i]);
            if (id >= 0 && hotAccepts(HOT_KIND[i], id) && p.hotSlotOf(id) < 0) p.hot[i] = id;
        }
    } else {
        std::vector<int> ws;
        for (int w = 0; w < W_COUNT; w++)
            if (p.wpn(w) > 0) ws.push_back(w);
        std::stable_sort(ws.begin(), ws.end(), [](int a, int b) { return WPN[a].dmg > WPN[b].dmg; });
        for (int w : ws) equipHotQuiet(itemWpn(w));
        for (int c = 0; c < C_COUNT; c++)
            if (p.inv.count(itemCons(c)) > 0) equipHotQuiet(itemCons(c));
    }
    // farming tools: the equipped ones live in the pack and on the hotbar. A save from before that kept them outside the pack, so
    // each equipped tool is put back in the pack (nothing lost) and onto the hotbar; a tool found on the hotbar counts as equipped.
    for (int t = 0; t < TT_COUNT; t++) {
        if (!isFarmType(t)) continue;
        if (p.tool[t] < 0)
            for (int i = 0; i < HOT_N; i++)
                if (p.hot[i] >= 0 && item(p.hot[i]).kind == IK_TOOL && TOOLDEF[item(p.hot[i]).ref].type == t && p.inv.count(p.hot[i]) > 0) p.tool[t] = item(p.hot[i]).ref;
    }
    for (int t = 0; t < TT_COUNT; t++)  // (the pack first: equipping one tool must not drop another that is not back in the pack yet)
        if (isFarmType(t) && p.tool[t] >= 0 && p.inv.count(itemTool(p.tool[t])) <= 0) p.inv.addIgnoringWeight(itemTool(p.tool[t]), 1);
    for (int t = 0; t < TT_COUNT; t++)
        if (isFarmType(t) && p.tool[t] >= 0) giveFarmTool(p.tool[t]);
    restoreMap();  // the home map is active after newGame(); other maps are put back when you travel there
    p.maxhp = (float)ARMOR_HP[p.up[U_ARMOR]];
    p.hp = p.maxhp;
    // keep a usable weapon selected (one that is equipped on the hotbar)
    fixSel();
    if (!canUse(p.sel))
        for (int w = 0; w < W_COUNT; w++)
            if (canUse(w)) {
                p.sel = w;
                break;
            }
    hsReindex();
    if (atMap != B_HOME) {
        travelTo(atMap, MAPS[atMap].entryTx, MAPS[atMap].entryTy, false);  // saved out in a biome: continue there
        if (atPosX >= 0 && atPosY >= 0) p.pos = freeSpot(atPosX / TS, atPosY / TS);  // saved inside a delve: back at its entrance
    } else if (atPosX >= 0 && atPosY >= 0) p.pos = freeSpot(atPosX / TS, atPosY / TS);  // saved inside a town building: back outside its door
    return true;
}
