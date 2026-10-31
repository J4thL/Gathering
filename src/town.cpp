// town.cpp - walking into the town's buildings and out again, and finding who and what is in reach inside.
//
// Entering parks everything of the outside (the map's state goes into mapMem as for any map change, the wildlife into extAnimals) and
// generates the interior (interiors.hpp) into the world; leaving regenerates the outside from the world seed, puts the parked state back
// and stands the hunter on the exact spot they entered from. The hunter's own state (pack, equipment, stats, skills, quests, time) is
// never touched by going in or out.
#include <algorithm>
#include "game.hpp"

namespace {
const char* NPC_NAME[BLD_COUNT] = {"BROKK", "HILD", "TOBIN", "GARRICK", "MARY ROSE", "SALONA MIA"};
const char* NPC_VERB[BLD_COUNT] = {"CRAFT", "CRAFT", "CRAFT", "SHOP", "FLOWERS", "STYLE"};
}  // namespace
const char* npcName(int k) { return NPC_NAME[k]; }

bool Game::nearShop() const { return interior >= 0; }

int Game::doorNear() const {
    if (interior >= 0 || world.mapId != B_HOME || p.dead) return -1;
    for (int k = 0; k < BLD_COUNT; k++)
        if (dist(p.pos, V2(bldDoorX(k, COX), bldDoorY(k, COY))) < BLD_DOOR_REACH) return k;
    return -1;
}

bool Game::nearExit() const { return interior >= 0 && dist(p.pos, world.exitPos) < 20.f; }

int Game::npcNear() const {
    if (interior < 0) return -1;
    int best = -1;
    float bd = 40.f;
    for (size_t i = 0; i < world.npcs.size(); i++) {
        const float d = dist(p.pos, world.npcs[i].pos);
        if (d < bd) bd = d, best = (int)i;
    }
    return best;
}

bool Game::stationNear() const {
    if (interior < 0) return false;
    for (const V2& s : world.stations)
        if (dist(p.pos, s) < 26.f) return true;
    return false;
}

bool Game::enterBuilding(int k) {
    if (k < 0 || k >= BLD_COUNT || interior >= 0 || p.dead) return false;
    snapshotMap();  // the outside: harvested resources, structures, discoveries, dropped weapons
    extMap = world.mapId;
    extPos = p.pos;  // the exact spot: leaving puts the hunter back here
    extAnimals = std::move(animals);
    animals.clear();
    structs.clear();
    chestOpen = -1;
    projs.clear();
    parts.clear();
    ftexts.clear();
    decals.clear();
    items.clear();
    world.generateInterior(k);
    initHarvest();
    interior = k;
    p.pos = world.spawn;
    p.kb = V2();
    p.iceV = V2();
    p.charging = false;
    p.gathering = false;
    p.gatherT = 0;
    hvKey = -1;
    bldCd = 0.4f;
    travelFade = 0.4f;
    setCam(0, true);
    audio.play(S_CLICK, 0.6f);
    return true;
}

bool Game::exitBuilding() {
    if (interior < 0) return false;
    const int map = extMap;
    structs.clear();
    projs.clear();
    parts.clear();
    ftexts.clear();
    items.clear();
    loadMapWorld(map);
    restoreMap();
    animals = std::move(extAnimals);
    extAnimals.clear();
    interior = -1;
    p.pos = extPos;
    p.kb = V2();
    p.iceV = V2();
    hvKey = -1;
    bldCd = 0.4f;
    travelFade = 0.4f;
    setCam(0, true);
    audio.play(S_CLICK, 0.6f);
    return true;
}

bool Game::townInteract() {
    if (bldCd > 0 || p.dead) return false;
    if (interior < 0) {
        const int k = doorNear();
        return k >= 0 && enterBuilding(k);
    }
    if (nearExit()) return exitBuilding();
    if (npcNear() >= 0 || stationNear()) {
        openService(interior);
        return true;
    }
    return false;
}

bool Game::townPrompt() {
    if (interior < 0) {
        const int k = doorNear();
        if (k < 0) return false;
        drawPrompt(bldDoorX(k, COX), bldDoorY(k, COY) - 6, "E", std::string("ENTER ") + BUILDINGS[k].name);
        return true;
    }
    if (nearExit()) {
        drawPrompt(world.exitPos.x, world.exitPos.y - 30, "E", "LEAVE");
        return true;
    }
    const int n = npcNear();
    if (n >= 0) {
        drawPrompt(world.npcs[n].pos.x, world.npcs[n].pos.y - 34, "E", std::string(NPC_VERB[interior]) + " WITH " + NPC_NAME[interior]);
        return true;
    }
    if (stationNear()) {
        drawPrompt(p.pos.x, p.pos.y - 30, "E", NPC_VERB[interior]);
        return true;
    }
    return false;
}
