// delve.cpp - going into and out of the Abandoned Mine and the Frozen Dungeon, and what happens inside while the hunter is there.
//
// Like a town building, a delve is a map of its own generated into the world (delve.hpp): entering parks the outside (the map's state goes
// into mapMem, the wildlife into extAnimals) and leaving puts it all back with the hunter on the spot they came in from. The mine is one map;
// the dungeon is four, joined by stairs, and each level remembers its opened chests, harvested ore and the fallen king between visits.
// The hunter's own state is never touched by going in or out.
#include <algorithm>
#include <sstream>
#include "core/game.hpp"

namespace {
std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    for (std::string l; std::getline(in, l);)
        if (!l.empty()) out.push_back(l);
    return out;
}
const int ARMOR_NEED[DV_COUNT] = {0, 0, 2, 4, 5};  // armour tier needed to enter each level (the dungeon's first level is open to anyone)
}

bool Game::delveReq(int id, std::string* why) const {
    if (id < 0 || id >= DV_COUNT) return false;
    const int ar = p.up[U_ARMOR];
    if (ar < ARMOR_NEED[id]) {
        if (why) *why = "NEED ARMOR TIER " + S(ARMOR_NEED[id]) + " (YOU HAVE " + S(ar) + ")";
        return false;
    }
    if (id == DV_FROST3 && !(p.has(CG_FROSTWARD) || ar >= 7)) {
        if (why) *why = "NEED A FROSTWARD CLOAK OR FROSTWARD ARMOR";
        return false;
    }
    return true;
}

void Game::ensureMonsterArt() {
    if (monsArtReady || !ren) return;
    buildMonsterBank(ren, A.M);
    monsArtReady = true;
}

void Game::resetDelveRuntime() {
    delve = -1;
    mons.clear();
    hazards.clear();
    mprojs.clear();
    dtraps.clear();
    dchests.clear();
    flow.clear();
    flowTx = flowTy = -1;
    bossIdx = -1;
    pSlowT = pPoisonT = 0;
    delveBanT = bossBanT = 0;
}

void Game::spawnDelve() {
    mons.clear();
    hazards.clear();
    mprojs.clear();
    dtraps.clear();
    dchests.clear();
    flow.clear();
    flowTx = flowTy = -1;
    bossIdx = -1;
    rockT = 6.f;
    DelveMem& dm = delveMem[delve];
    for (size_t i = 0; i < world.dsp.size(); i++) {
        const DSpawn& s = world.dsp[i];
        if (s.type == DS_MONSTER) spawnMonster(s.kind, V2(s.x, s.y), s.depth, s.elite, (int)i);
        else if (s.type == DS_BOSS) {
            if (!dm.bossDead) spawnMonster(MK_KING, V2(s.x, s.y), 1.f, false, (int)i);
        } else if (s.type == DS_TRAP) {
            DTrap t;
            t.kind = s.kind;
            t.pos = V2(s.x, s.y);
            t.t = rng.range(0.f, TRAP_CYCLE);
            t.depth = s.depth;
            dtraps.push_back(t);
        } else if (s.type == DS_CHEST) {
            if (s.id == DELVE_BOSS_CHEST ? !dm.bossDead : false) continue;  // the king's chest appears when he falls
            if (dm.chests & (1u << s.id)) continue;
            DChest c;
            c.pos = V2(s.x, s.y);
            c.tier = s.kind;
            c.id = s.id;
            c.locked = s.elite;
            dchests.push_back(c);
        }
    }
    // the first sentry of the dungeon's last level is asleep until the hunter is near; nothing else to set up
}

// ------------------------------------------------------------------ going in and out
bool Game::enterDelve(int id) {
    if (id < 0 || id >= DV_COUNT || delve >= 0 || interior >= 0 || p.dead) return false;
    std::string why;
    if (!delveReq(id, &why)) {
        msg(why, C_RED);
        audio.play(S_ERROR, 0.6f);
        bldCd = 0.5f;
        return false;
    }
    ensureMonsterArt();
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
    world.generateDelve(id);
    initHarvest();
    delve = id;
    DelveMem& dm = delveMem[id];
    if (dm.visited) harvestLoad(dm.hv);
    dm.visited = true;
    spawnDelve();
    p.pos = world.spawn;
    p.kb = V2();
    p.iceV = V2();
    p.charging = false;
    p.gathering = false;
    p.gatherT = 0;
    pSlowT = pPoisonT = 0;
    hvKey = -1;
    bldCd = 0.5f;
    travelFade = 0.8f;
    delveBanT = 3.5f;
    delveKills = 0;
    setCam(0, true);
    audio.play(S_FELL, 0.5f);
    return true;
}

bool Game::leaveDelve() {
    if (delve < 0) return false;
    {
        std::ostringstream h;
        harvestSave(h);
        delveMem[delve].hv = splitLines(h.str());
    }
    const int map = extMap;
    structs.clear();
    projs.clear();
    parts.clear();
    ftexts.clear();
    items.clear();
    hazards.clear();
    mprojs.clear();
    mons.clear();
    dtraps.clear();
    dchests.clear();
    loadMapWorld(map);
    restoreMap();
    animals = std::move(extAnimals);
    extAnimals.clear();
    delve = -1;
    bossIdx = -1;
    pSlowT = pPoisonT = 0;
    p.pos = extPos;
    p.kb = V2();
    p.iceV = V2();
    hvKey = -1;
    bldCd = 0.5f;
    travelFade = 0.6f;
    travelCd = 0.8f;
    setCam(0, true);
    audio.play(S_CLICK, 0.6f);
    return true;
}

// stairs: the dungeon's levels are maps of their own; the hunter arrives beside the opposite staircase
bool Game::delveGo(int id, bool down) {
    if (delve < 0 || id < 0 || id >= DV_COUNT || p.dead) return false;
    std::string why;
    if (!delveReq(id, &why)) {
        msg(why, C_RED);
        audio.play(S_ERROR, 0.6f);
        bldCd = 0.5f;
        return false;
    }
    {
        std::ostringstream h;
        harvestSave(h);
        delveMem[delve].hv = splitLines(h.str());
    }
    projs.clear();
    parts.clear();
    ftexts.clear();
    items.clear();
    decals.clear();
    world.generateDelve(id);
    initHarvest();
    delve = id;
    DelveMem& dm = delveMem[id];
    if (dm.visited) harvestLoad(dm.hv);
    dm.visited = true;
    spawnDelve();
    const V2 at = down ? world.stairsUp : world.stairsDn;
    p.pos = freeSpot((int)floorf((at.x + (down ? 18.f : 18.f)) / TS), (int)floorf(at.y / TS));
    p.kb = V2();
    p.iceV = V2();
    p.charging = false;
    p.gathering = false;
    hvKey = -1;
    pSlowT = pPoisonT = 0;
    bldCd = 0.5f;
    travelFade = 0.6f;
    delveBanT = 3.5f;
    setCam(0, true);
    audio.play(S_FELL, 0.4f);
    return true;
}

// ------------------------------------------------------------------ what E does here
int Game::delveNear(V2* at) const {
    if (p.dead) return 0;
    auto near = [&](V2 t, float r) { return t.x >= 0 && dist(p.pos, t) < r; };
    const_cast<Game*>(this)->delveNearIdx = -1;
    if (delve < 0) {
        if (interior < 0 && near(world.gate, 26.f)) {
            if (at) *at = world.gate;
            return 1;
        }
        return 0;
    }
    for (size_t i = 0; i < dchests.size(); i++)
        if (dist(p.pos, dchests[i].pos) < 30.f) {
            const_cast<Game*>(this)->delveNearIdx = (int)i;
            if (at) *at = dchests[i].pos;
            return 6;
        }
    if (near(world.bossDoor, 28.f)) {
        for (const Prop& pr : world.props)
            if (pr.kind == P_BOSSDOOR && pr.ph < 0.5f) {
                if (at) *at = world.bossDoor;
                return 5;
            }
    }
    if (near(world.stairsDn, 24.f)) { if (at) *at = world.stairsDn; return 3; }
    if (near(world.stairsUp, 24.f) && delveIsFrost(delve) && delve > DV_FROST0) { if (at) *at = world.stairsUp; return 4; }
    if (near(world.exitPos, 24.f)) { if (at) *at = world.exitPos; return 2; }
    return 0;
}

static const char* chestName(int tier) {
    static const char* N[4] = {"OLD CHEST", "IRON-BOUND CHEST", "ANCIENT CHEST", "THE KING'S TREASURE"};
    return N[std::max(0, std::min(3, tier))];
}

void Game::openDelveChest(int i) {
    if (i < 0 || i >= (int)dchests.size()) return;
    const DChest c = dchests[i];
    if (c.locked) {
        if (p.mat(M_CRYPTKEY) < 1) {
            msg("LOCKED - NEEDS A CRYPT KEY (THE CRYPT'S ELITES CARRY THEM)", C_RED);
            audio.play(S_ERROR, 0.6f);
            bldCd = 0.5f;
            return;
        }
        p.inv.remove(itemMat(M_CRYPTKEY), 1);
        msg("THE KEY TURNS", C_GOLD);
    }
    auto give = [&](int itemId, int n) {
        if (n <= 0) return;
        const int got = p.inv.add(itemId, n);
        if (got < n) {  // pack full or too heavy: the rest spills on the floor, nothing is lost
            GroundItem g{};
            g.pos = c.pos + V2(rng.range(-10, 10), rng.range(8, 18));
            g.item = itemId;
            g.n = n - got;
            g.ammo = -1;
            g.lock = 0.6f;
            items.push_back(g);
        }
        ftext(p.pos + V2(rng.range(-8, 8), -24 - 7 * (int)ftexts.size() % 28), "+" + S(n) + " " + item(itemId).name, C_GOLD, 1.5f);
    };
    auto rnd = [&](int lo, int hi) { return rng.irange(lo, hi); };
    int coins = 0;
    switch (c.tier) {
        case 0:
            coins = rnd(25, 70);
            give(itemMat(M_COAL), rnd(2, 4));
            give(itemMat(M_ROPE), rnd(1, 2));
            give(itemCons(C_BANDAGE), rnd(1, 2));
            if (rng.chance(0.5f)) give(itemMat(M_COPPERBAR), 1);
            break;
        case 1:
            coins = rnd(70, 160);
            give(itemMat(M_IRONBAR), rnd(1, 2));
            give(itemMat(M_SILVER), rnd(1, 3));
            give(itemCons(C_BANDAGE), 2);
            if (rng.chance(0.25f)) give(itemMat(M_GEM), 1);
            if (rng.chance(0.4f)) give(itemMat(M_ICESHARD), rnd(1, 3));
            break;
        case 2:
            coins = rnd(160, 380);
            give(itemMat(M_SILVERBAR), rnd(1, 2));
            give(itemMat(M_STEELBAR), rnd(1, 2));
            give(itemMat(M_FROSTRUNE), rnd(1, 2));
            give(itemMat(M_ICEBONE), rnd(1, 3));
            if (rng.chance(0.35f)) give(itemCons(C_TONIC), 1);
            if (rng.chance(0.4f)) give(itemMat(M_GEM), 1);
            if (rng.chance(0.25f)) give(itemMat(M_RARECRYSTAL), 1);
            break;
        default:  // the king's hoard
            coins = 800;
            give(itemCarry(CG_CROWN), 1);
            give(itemCarry(CG_HEART), 1);
            give(itemWpn(W_FROSTBRAND), 1);
            give(itemMat(M_RARECRYSTAL), 3);
            give(itemMat(M_GEM), 4);
            give(itemMat(M_SILVERBAR), 3);
            break;
    }
    p.coins += coins;
    ftext(c.pos + V2(0, -24), "+" + S(coins) + " COINS", C_GOLD, 1.3f);
    msg(std::string("OPENED THE ") + chestName(c.tier) + ": +" + S(coins) + " COINS", C_GOLD);
    audio.play(S_COIN);
    puff(c.pos + V2(0, -8), 10, c.tier >= 2 ? HEX(0xdff6ff) : HEX(0xffe08a));
    DelveMem& dm = delveMem[delve];
    dm.chests |= 1u << c.id;
    if (c.id != DELVE_BOSS_CHEST && dm.chestT <= 0) dm.chestT = 1800.f;  // the room's chests refill after a while
    dchests.erase(dchests.begin() + i);
    bldCd = 0.4f;
}

bool Game::delveInteract() {
    if (bldCd > 0 || p.dead) return false;
    V2 at;
    const int k = delveNear(&at);
    switch (k) {
        case 1: return enterDelve(world.mapId == B_SNOW ? DV_FROST0 : DV_MINE);
        case 2: return leaveDelve();
        case 3: return delveGo(delve + 1, true);
        case 4: return delveGo(delve - 1, false);
        case 5:
            for (Prop& pr : world.props)
                if (pr.kind == P_BOSSDOOR) pr.ph = 1.f;
            for (int x = 64; x <= 66; x++) world.solid[92][x] = false;
            msg("THE ANCIENT DOOR GRINDS OPEN...", C_BLUE);
            audio.play(S_FELL, 0.8f);
            shakeAmt = 4.f;
            bldCd = 0.5f;
            return true;
        case 6: openDelveChest(delveNearIdx); return true;
    }
    return false;
}

bool Game::delvePrompt() {
    V2 at;
    const int k = delveNear(&at);
    if (!k) return false;
    std::string label;
    float dy = -30.f;
    switch (k) {
        case 1: {
            const int id = world.mapId == B_SNOW ? DV_FROST0 : DV_MINE;
            label = std::string("ENTER ") + DELVE_NAME[id];
            break;
        }
        case 2: label = delve == DV_MINE || delve == DV_FROST0 ? "LEAVE" : "GO BACK"; break;
        case 3: {
            std::string why;
            if (delveReq(delve + 1, &why)) label = std::string("DESCEND TO ") + DELVE_NAME[delve + 1];
            else label = why;
            break;
        }
        case 4: label = std::string("ASCEND TO ") + DELVE_NAME[delve - 1]; break;
        case 5: label = "OPEN THE DOOR TO THE THRONE ROOM"; break;
        case 6: {
            const DChest& c = dchests[delveNearIdx];
            label = c.locked ? (p.mat(M_CRYPTKEY) > 0 ? "UNLOCK WITH CRYPT KEY" : "LOCKED - CRYPT KEY NEEDED") : std::string("OPEN ") + chestName(c.tier);
            dy = -24.f;
            break;
        }
    }
    drawPrompt(at.x, at.y + dy, "E", label);
    return true;
}

// ------------------------------------------------------------------ time
void Game::tickDelveTimers(float dt) {
    for (int i = 0; i < DV_COUNT; i++) {
        DelveMem& d = delveMem[i];
        if (d.bossDead) {
            d.bossTimer -= dt;
            if (d.bossTimer <= 0) {  // the king returns and the dungeon's treasure is replaced
                d.bossDead = false;
                d.chests = 0;
                if (delve == i) msg("A COLD WIND... THE KING STIRS AGAIN", C_BLUE);
            }
        }
        if (d.chestT > 0) {
            d.chestT -= dt;
            if (d.chestT <= 0) d.chests &= (1u << DELVE_BOSS_CHEST);
        }
    }
    if (p.has(CG_HEART) && !p.dead && p.hp < p.maxhp) p.hp = std::min(p.maxhp, p.hp + 0.7f * dt);  // the frozen heart mends slowly
}

void Game::updateDelve(float dt) {
    if (delveBanT > 0) delveBanT -= dt;
    if (bossBanT > 0) bossBanT -= dt;
    if (pSlowT > 0) pSlowT -= dt;
    if (pPoisonT > 0 && !p.dead) {
        pPoisonT -= dt;
        static float acc = 0;
        acc += dt;
        if (acc >= 1.f) {  // poison ticks, but never kills on its own
            acc = 0;
            if (p.hp > 3) {
                p.hp -= 2;
                ftext(p.pos + V2(0, -22), "-2", HEX(0x7ad04a), 0.7f);
            }
        }
    }
    updateMonsters(dt);
    updateHazards(dt);
    updateMProjs(dt);
    updateTraps(dt);
    // the mine: now and then a stone comes loose near the hunter (a dusty circle warns first)
    if (delve == DV_MINE && !p.dead) {
        rockT -= dt;
        if (rockT <= 0) {
            rockT = rng.range(7.f, 14.f);
            for (int tries = 0; tries < 6; tries++) {
                const V2 at = p.pos + V2(rng.range(-60, 60), rng.range(-40, 40));
                if (world.blocked(at.x, at.y, 8.f)) continue;
                addHazard(HZ_ROCK, at, 11.f, 1.0f, 16.f * (1.f + 0.5f * std::min(1.f, dist(world.spawn, p.pos) / 900.f)));
                break;
            }
        }
    }
    // drifting dust (mine) / ice motes (dungeon) in the dark
    ambT -= dt;
    if (ambT <= 0) {
        ambT = 0.12f;
        const bool frost = delveIsFrost(delve);
        parts.push_back({p.pos.x + rng.range(-150, 150), p.pos.y + rng.range(-90, 90), rng.range(8, 30), rng.range(-6, 6), rng.range(-3, 3), rng.range(-4, 2), 2.6f, 2.6f,
                         frost ? HEX(0xcfeaff) : HEX(0xb8a888), 1, false});
    }
}

// ------------------------------------------------------------------ saving
std::string Game::delveSaveLines() {
    if (delve >= 0) {  // the level the hunter is in: its harvested ore is in the live world
        std::ostringstream h;
        harvestSave(h);
        delveMem[delve].hv = splitLines(h.str());
    }
    std::ostringstream f;
    for (int i = 0; i < DV_COUNT; i++) {
        const DelveMem& d = delveMem[i];
        if (!d.visited) continue;
        f << "delve " << i << ' ' << d.chests << ' ' << (d.bossDead ? 1 : 0) << ' ' << (int)d.bossTimer << ' ' << (int)d.chestT << "\n";
        for (auto& ln : d.hv) f << "dhv " << i << ' ' << ln << "\n";
    }
    return f.str();
}
