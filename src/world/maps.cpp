// maps.cpp - travelling between the five maps (maps.hpp). `world` holds the active map; travelTo parks the state that belongs to
// the map being left (placed structures and chests, harvested resources, discovered landmarks, dropped weapons) in mapMem,
// regenerates the destination from the world seed, puts its parked state back and places the hunter on a free tile.
// Nothing that belongs to the hunter (health, pack, equipment, skills, quests, time of day) is touched by a trip.
#include <algorithm>
#include <sstream>
#include "core/game.hpp"

namespace {
std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string ln;
    while (std::getline(in, ln))
        if (!ln.empty()) out.push_back(ln);
    return out;
}
}  // namespace

void Game::loadMapWorld(int map) {
    world.generate(WORLD_SEED, map);
    if (map == B_HOMESTEAD) hsApplyWorld();
    initHarvest();
    if (ren) {  // the minimap shows the active map
        if (miniTex) SDL_DestroyTexture(miniTex);
        miniTex = toTex(ren, world.minimap());
    }
}

// Everything done on the way into a building or a delve: the outside is parked (harvested resources, structures, discoveries, dropped
// weapons), the exact spot is remembered (leaving puts the hunter back there) and what lives only outside is cleared.
void Game::stashOutside() {
    snapshotMap();
    extMap = world.mapId;
    extPos = p.pos;
    extAnimals = std::move(animals);
    animals.clear();
    structs.clear();
    chestOpen = -1;
    projs.clear();
    parts.clear();
    ftexts.clear();
    decals.clear();
    items.clear();
}

void Game::snapshotMap() {
    if (world.isInterior()) return;  // an interior has no state of its own: the outside was parked when the hunter went in
    MapMem& m = mapMem[world.mapId];
    m.visited = true;
    std::ostringstream h, s;
    harvestSave(h);
    structSave(s);
    m.hv = splitLines(h.str());
    m.st = splitLines(s.str());
    m.disc.clear();
    for (auto& l : world.landmarks) m.disc += l.found ? '1' : '0';
    m.items = items;
    m.seen = seen;
}

void Game::restoreMap() {
    if (world.isInterior()) return;
    const MapMem& m = mapMem[world.mapId];
    structs.clear();
    harvestLoad(m.hv);
    structLoad(m.st);
    for (size_t i = 0; i < m.disc.size() && i < world.landmarks.size(); i++) world.landmarks[i].found = m.disc[i] == '1';
    items = m.items;
    seen = m.seen.size() == (size_t)MM_W * MM_H ? m.seen : std::string((size_t)MM_W * MM_H, '0');
}

void Game::revealAround(V2 pos) {
    if (seen.size() != (size_t)MM_W * MM_H) seen.assign((size_t)MM_W * MM_H, '0');
    const int R = 9;  // minimap cells (about 27 tiles, a bit more than the screen)
    const int cx = (int)(pos.x / TS / MM_DIV), cy = (int)(pos.y / TS / MM_DIV);
    for (int y = std::max(0, cy - R); y <= std::min(MM_H - 1, cy + R); y++)
        for (int x = std::max(0, cx - R); x <= std::min(MM_W - 1, cx + R); x++)
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= R * R) seen[(size_t)y * MM_W + x] = '1';
}

// The nearest tile (searching outwards from the wanted one) the hunter can stand on, as a position. Never outside the map's wall.
V2 Game::freeSpot(int tx, int ty) const {
    auto ok = [&](int x, int y) {
        if (x < 3 || y < 3 || x >= MW - 3 || y >= MH - 3) return false;
        const V2 c(x * TS + 8.f, y * TS + 8.f);
        return !world.blocked(c.x, c.y, 8.f) && !tileDeep(world.tile[y][x]);
    };
    for (int rad = 0; rad <= 14; rad++)
        for (int dy = -rad; dy <= rad; dy++)
            for (int dx = -rad; dx <= rad; dx++) {
                if (std::max(abs(dx), abs(dy)) != rad) continue;
                if (ok(tx + dx, ty + dy)) return V2((tx + dx) * TS + 8.f, (ty + dy) * TS + 8.f);
            }
    // nothing near the wanted tile: any free tile on the map (this cannot normally happen)
    for (int y = 3; y < MH - 3; y++)
        for (int x = 3; x < MW - 3; x++)
            if (ok(x, y)) return V2(x * TS + 8.f, y * TS + 8.f);
    return V2(MW * TS / 2.f, MH * TS / 2.f);
}

bool Game::travelTo(int map, int atx, int aty, bool autosave) {
    if (map < 0 || map >= B_COUNT || p.dead) return false;
    resetDelveRuntime();
    snapshotMap();
    structs.clear();
    chestOpen = -1;
    animals.clear();  // wildlife belongs to the place it lives in; home herds are repopulated, biome wildlife streams in
    projs.clear();
    parts.clear();
    ftexts.clear();
    decals.clear();
    items.clear();
    loadMapWorld(map);
    restoreMap();
    p.pos = freeSpot(atx, aty);
    p.kb = V2();
    p.iceV = V2();
    p.charging = false;
    p.gathering = false;
    p.gatherT = 0;
    hvKey = -1;
    curBiome = pendBiome = map;
    bannerBiome = map;
    bannerT = 3.2f;
    pendT = 0;
    bannerSub = BIOME[map].climate[0] ? std::string("CLIMATE: ") + BIOME[map].climate : std::string("");
    for (int b = 0; b < B_COUNT; b++) bw[b] = b == map ? 1.f : 0.f;
    for (int k = 0; k < A_COUNT; k++) spCool[k] = 0;
    spawnT = 0;
    respawnT = 5;
    if (map == B_HOME) populate();
    travelFade = 0.6f;
    travelCd = 1.5f;
    setCam(0, true);
    audio.play(S_PICKUP, 0.5f);
    if (autosave) save();
    return true;
}

// Walking into a gate gap travels to the map behind it.
void Game::checkPortals(float dt) {
    if (travelCd > 0) travelCd -= dt;
    if (travelFade > 0) travelFade -= dt;
    if (p.dead || travelCd > 0 || interior >= 0 || delve >= 0) return;
    const Portal* pt = portalAt(world.mapId, (int)floorf(p.pos.x / TS), (int)floorf(p.pos.y / TS));
    if (!pt) return;
    if (!gateOpen[pt->toMap]) {  // a sealed gate asks for an offering first
        openGatePanel(pt->toMap, pt->side);
        return;
    }
    travelTo(pt->toMap, pt->arriveTx, pt->arriveTy);
}
