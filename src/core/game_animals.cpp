// game_animals.cpp - wild animals: spawning, rarity tiers, AI, hits, kills and carcasses.
#include <cstdio>
#include <fstream>
#include <sstream>
#include "core/game.hpp"
#include "core/logic_util.hpp"

// ================================================================= animals
// ---- the home forest's wildlife: where each species likes to be, and how many of it are about at each hour
// How much a species likes the ground at tile (tx, ty), 0..1 (up to 3 in the glades the herbivores favour): rabbits and deer graze the meadows,
// leaf litter and glades and deer drink at the water; boars root about in the thickets, moss and root-tangled ground; wolves keep to needles,
// roots and dark thickets by day and range out over the meadows at night. Boars and wolves shy away from the open glades.
static float zoneWeight(const World& w, int kind, int tx, int ty, float night) {
    const uint8_t t = w.tile[ty][tx];
    bool glade = false, field = false;
    for (const Glade& g : w.glades)
        if (hypotf((tx - g.cx) / g.rx, (ty - g.cy) / g.ry) < 0.9f) { glade = true; break; }
    for (const FlowerField& g : w.fields)
        if (hypotf((tx - g.cx) / g.rx, (ty - g.cy) / g.ry) < 0.85f) { field = true; break; }
    float f = 0.3f;
    switch (kind) {
        case A_RABBIT:
            f = t == T_GRASS ? 1.f : t == T_LEAF ? 0.7f : t == T_MOSS ? 0.5f : t == T_DARK ? 0.35f : t == T_NEEDLES ? 0.2f : t == T_ROOTS ? 0.1f : 0.3f;
            if (glade) f *= 3.f;
            if (field) f *= 2.5f;
            break;
        case A_DEER: {
            f = t == T_GRASS ? 1.f : t == T_LEAF ? 0.8f : t == T_MOSS ? 0.6f : t == T_DARK ? 0.4f : t == T_NEEDLES ? 0.25f : t == T_ROOTS ? 0.1f : 0.3f;
            if (glade) f *= 3.f;
            if (field) f *= 2.5f;
            bool water = false;  // deer like to be near water (a lake or a brook within 3 tiles)
            for (int dy = -3; dy <= 3 && !water; dy++)
                for (int dx = -3; dx <= 3; dx++) {
                    const int x = tx + dx, y = ty + dy;
                    if (x >= 0 && y >= 0 && x < MW && y < MH && (tileLake(w.tile[y][x]) || w.tile[y][x] == T_SHALLOW)) { water = true; break; }
                }
            if (water) f *= 1.5f;
            break;
        }
        case A_BOAR:
            f = t == T_DARK ? 1.f : t == T_MOSS ? 0.9f : t == T_ROOTS ? 0.8f : t == T_LEAF ? 0.7f : t == T_NEEDLES ? 0.4f : t == T_GRASS ? 0.25f : 0.2f;
            if (glade) f *= 0.3f;
            if (field) f *= 0.5f;
            break;
        case A_WOLF:
            f = t == T_NEEDLES ? 1.f : t == T_ROOTS ? 0.9f : t == T_DARK ? 0.7f : t == T_LEAF ? 0.4f : t == T_MOSS ? 0.3f : t == T_GRASS ? 0.1f + 0.55f * night : 0.1f;
            if (glade) f *= 0.3f;
            if (field) f *= 0.5f;
            break;
    }
    return f;
}
static const float ZONE_MAX[4] = {3.f, 4.5f, 1.f, 1.f};  // the largest zoneWeight a species can get (to turn a weight into a chance)

// How many of a species the home forest holds at this hour, out of its full herd (HOME_HERDS). Rabbits keep to the daylight, deer are busiest
// at dawn and dusk, boars are about at all hours (a few more at night), wolves hunt at night and are scarce by day.
int Game::homeTarget(int kind) const {
    if (kind < 0 || kind > 3) return 0;
    const float day = daylight(), night = 1.f - day;
    const float crep = 1.f - smoothf(0.f, 0.45f, fabsf(sinf(tod() * 6.2832f) - 0.05f));  // 1 at dawn and dusk, 0 by noon and midnight
    float f = 1.f;
    switch (kind) {
        case A_RABBIT: f = 0.45f + 0.55f * day + 0.15f * crep; break;
        case A_DEER: f = 0.55f + 0.25f * day + 0.45f * crep; break;
        case A_BOAR: f = 0.9f + 0.25f * night; break;
        case A_WOLF: f = 0.45f + 1.15f * night; break;
    }
    return std::max(1, (int)lroundf(HOME_HERDS[kind] * f));
}

// When a species has more members out than the hour calls for (the sun came up on the wolves, the night fell on the rabbits), one of them
// heads home: a common specimen, far from the player and out of sight, and not in the middle of a fight or a flight. It walks to its bed
// and is gone once it is there and out of sight (see performAct).
void Game::cullHome(int kind) {
    Animal* best = nullptr;
    float bd = 300.f;
    for (auto& a : animals) {
        if (a.dead || a.gone || a.leaving || a.kind != kind || a.biome != B_HOME || a.tier >= TR_RARE || a.hp < a.maxhp || a.tracked) continue;
        if (a.st != AS_WANDER && a.st != AS_FEED && a.st != AS_REST) continue;
        const float d = dist(a.pos, p.pos);
        const float sx = a.pos.x - cam.x, sy = a.pos.y - cam.y;
        if (sx > -80 && sx < LW + 80 && sy > -90 && sy < LH + 90) continue;
        if (d > bd) { bd = d; best = &a; }
    }
    if (best) best->leaving = true;
}


// ---- daily routines (the home forest's four species): each has a bed (a sheltered spot in ground it likes) and, for deer, a watering place,
// and its day is made of acts: sleep at the bed, graze, drink, forage, patrol, roam. Which act it picks depends on the species and the hour.
static bool inGladeAt(const World& w, int tx, int ty) {
    for (const Glade& g : w.glades) if (hypotf((tx - g.cx) / g.rx, (ty - g.cy) / g.ry) < 0.9f) return true;
    return false;
}
static bool inFieldAt(const World& w, int tx, int ty) {
    for (const FlowerField& g : w.fields) if (hypotf((tx - g.cx) / g.rx, (ty - g.cy) / g.ry) < 0.85f) return true;
    return false;
}
static bool routineAnimal(const Animal& a) { return a.kind < 4 && a.biome == B_HOME; }

int Game::hourOfDay() const {
    const float t = tod() * 6.2832f, s = sinf(t);
    if (s > 0.35f) return H_DAY;
    if (s < -0.25f) return H_NIGHT;
    return cosf(t) > 0.f ? H_DAWN : H_DUSK;  // the sun is rising (dawn) or setting (dusk)
}

// A bed: the best of 60 spots within ~260 px, scored by the ground the species sleeps on and by how much cover (trees, rocks) is round it;
// the open glades and flower fields are no place to sleep.
bool Game::assignBed(Animal& a) {
    float bestScore = -1.f;
    V2 best = a.pos;
    for (int i = 0; i < 60; i++) {
        const V2 c = a.pos + V2(frng.range(-260.f, 260.f), frng.range(-260.f, 260.f));
        const int tx = (int)(c.x / TS), ty = (int)(c.y / TS);
        if (tx < 4 || ty < 4 || tx >= MW - 4 || ty >= MH - 4 || world.solidTile(tx, ty) || world.inTown(c.x, c.y, 48.f) || world.biomeAtPos(c.x, c.y) != a.biome) continue;
        const uint8_t t = world.tile[ty][tx];
        float lik;
        switch (a.kind) {
            case A_RABBIT: lik = t == T_DARK ? 0.8f : t == T_MOSS ? 0.7f : t == T_NEEDLES ? 0.6f : t == T_ROOTS ? 0.5f : t == T_LEAF ? 0.4f : t == T_GRASS ? 0.1f : 0.f; break;
            case A_DEER: lik = t == T_LEAF ? 1.f : t == T_DARK ? 0.8f : t == T_MOSS ? 0.7f : t == T_NEEDLES ? 0.5f : t == T_GRASS ? 0.2f : 0.f; break;
            case A_BOAR: lik = t == T_ROOTS ? 1.f : t == T_MOSS ? 0.9f : t == T_DARK ? 0.8f : t == T_NEEDLES ? 0.6f : t == T_LEAF ? 0.5f : t == T_GRASS ? 0.1f : 0.f; break;
            default: lik = t == T_NEEDLES ? 1.f : t == T_ROOTS ? 0.9f : t == T_DARK ? 0.7f : t == T_LEAF ? 0.4f : t == T_MOSS ? 0.3f : t == T_GRASS ? 0.05f : 0.f; break;  // wolf
        }
        if (lik <= 0.f || inGladeAt(world, tx, ty) || inFieldAt(world, tx, ty)) continue;
        int cover = 0;
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++) cover += world.sightBlock(tx + dx, ty + dy) && (dx || dy);
        const float score = lik + std::min(cover, 6) * 0.12f;
        if (score > bestScore) { bestScore = score; best = c; }
    }
    a.bed = best;
    a.hasBed = bestScore > 0.f;
    return a.hasBed;
}

// A watering place: the nearest open bank within ~500 px (a walkable tile with a lake or brook within 2 tiles).
bool Game::assignDrink(Animal& a) {
    float bd = 1e9f;
    for (int i = 0; i < 110; i++) {
        const V2 c = a.pos + V2(frng.range(-500.f, 500.f), frng.range(-500.f, 500.f));
        const int tx = (int)(c.x / TS), ty = (int)(c.y / TS);
        if (tx < 4 || ty < 4 || tx >= MW - 4 || ty >= MH - 4 || world.solidTile(tx, ty) || world.inTown(c.x, c.y, 48.f)) continue;
        const uint8_t t = world.tile[ty][tx];
        if (t == T_WATER || t == T_SHALLOW) continue;
        bool wet = false;
        for (int dy = -2; dy <= 2 && !wet; dy++)
            for (int dx = -2; dx <= 2; dx++) {
                const uint8_t o = world.tile[ty + dy][tx + dx];
                if (o == T_WATER || o == T_SHALLOW) { wet = true; break; }
            }
        const float d = dist(c, a.pos);
        if (wet && d < bd) { bd = d; a.drink = c; }
    }
    a.hasDrink = bd < 1e8f;
    return a.hasDrink;
}

// the best of a few spots round `from` by how much the species likes the ground there (the placement weights, see zoneWeight)
static V2 bestSpot(const World& w, int kind, V2 from, float range, int tries, float night, Rng& rg) {
    V2 best = from;
    float bw = -1.f;
    for (int i = 0; i < tries; i++) {
        const V2 c = from + V2(rg.range(-range, range), rg.range(-range, range));
        const int tx = (int)(c.x / TS), ty = (int)(c.y / TS);
        if (tx < 4 || ty < 4 || tx >= MW - 4 || ty >= MH - 4 || w.solidTile(tx, ty) || w.inTown(c.x, c.y, 48.f)) continue;
        const float wt = zoneWeight(w, kind, tx, ty, night) * (0.7f + 0.6f * rg.f());
        if (wt > bw) { bw = wt; best = c; }
    }
    return best;
}

void Game::chooseAct(Animal& a) {
    const int hr = hourOfDay();
    const float roll = frng.f(), night = 1.f - daylight();
    int act = ACT_ROAM;
    if (a.pack >= 0 && !a.leader && !a.leaving) {  // a follower keeps to its leader: the same act, at the leader's place (spread by its slot)
        Animal* L = packLeader(a.pack);
        if (!L) { promotePack(a.pack); L = packLeader(a.pack); }
        if (L && L != &a) {
            const float ang = a.packSlot * 2.1f + 0.7f;
            const V2 off(cosf(ang) * 26.f, sinf(ang) * 18.f);
            a.actArrived = false;
            a.stT = 0;
            a.idleT = 0;
            if (dist(a.pos, L->pos) > 200.f) {  // it has fallen behind: catch up
                a.act = ACT_ROAM;
                a.target = L->pos + off;
                return;
            }
            a.act = L->act == ACT_LEAVE ? ACT_ROAM : L->act;
            a.target = (a.act == ACT_SLEEP && L->hasBed ? L->bed : L->target) + off;
            if (a.act == ACT_SLEEP) { a.bed = a.target; a.hasBed = true; }
            return;
        }
    }
    if (a.leaving) act = ACT_LEAVE;
    else if (a.hp < a.maxhp * 0.6f) act = ACT_SLEEP;  // hurt: back to the bed to heal
    else
        switch (a.kind) {
            case A_RABBIT:  // sleeps in its burrow at night; grazes all day (with a nap), busiest at dawn and dusk
                if (hr == H_NIGHT) act = ACT_SLEEP;
                else if (hr == H_DAY) act = roll < 0.12f ? ACT_SLEEP : roll < 0.80f ? ACT_GRAZE : ACT_ROAM;
                else act = roll < 0.80f ? ACT_GRAZE : ACT_ROAM;
                break;
            case A_DEER:  // drinks and grazes at dawn and dusk, lies up in the thicket at midday and at night
                if (hr == H_NIGHT) act = ACT_SLEEP;
                else if (hr == H_DAWN || hr == H_DUSK) act = roll < 0.30f ? ACT_DRINK : roll < 0.85f ? ACT_GRAZE : ACT_ROAM;
                else act = roll < 0.50f ? ACT_SLEEP : roll < 0.78f ? ACT_GRAZE : ACT_DRINK;
                break;
            case A_BOAR:  // roots about in the thickets at all hours; wallows at its bed in the heat of the day
                if (hr == H_DAY) act = roll < 0.35f ? ACT_SLEEP : roll < 0.90f ? ACT_FORAGE : ACT_ROAM;
                else act = roll < 0.85f ? ACT_FORAGE : ACT_ROAM;
                break;
            default:  // wolf: sleeps by its den by day, goes back to it at dawn, makes its rounds at dusk and by night
                if (hr == H_DAY) act = roll < 0.80f ? ACT_SLEEP : ACT_ROAM;
                else if (hr == H_DAWN) act = roll < 0.60f ? ACT_SLEEP : ACT_PATROL;
                else act = roll < 0.80f ? ACT_PATROL : ACT_ROAM;
                break;
        }
    if ((act == ACT_SLEEP || act == ACT_LEAVE) && !a.hasBed && !assignBed(a)) act = ACT_ROAM;
    if (act == ACT_DRINK && !a.hasDrink && !assignDrink(a)) act = ACT_GRAZE;
    V2 target = a.pos;
    switch (act) {
        case ACT_SLEEP: case ACT_LEAVE: target = a.bed; break;
        case ACT_DRINK: target = a.drink; break;
        case ACT_GRAZE: target = bestSpot(world, a.kind, a.pos, 200.f, 8, night, frng); break;
        case ACT_FORAGE: target = bestSpot(world, a.kind, a.pos, 170.f, 8, night, frng); break;
        case ACT_PATROL: target = bestSpot(world, a.kind, a.pos, 320.f, 8, night, frng); break;
        default: target = a.pos + V2(frng.range(-90.f, 90.f), frng.range(-70.f, 70.f)); break;
    }
    a.act = act;
    a.actArrived = false;
    a.target = target;
    a.stT = 0;
    a.idleT = 0;
}

void Game::performAct(Animal& a) {
    a.actArrived = true;
    switch (a.act) {
        case ACT_SLEEP: a.st = AS_REST; a.stT = frng.range(25.f, 55.f); break;
        case ACT_DRINK: a.st = AS_FEED; a.stT = frng.range(5.f, 9.f); break;
        case ACT_GRAZE: a.st = AS_FEED; a.stT = frng.range(6.f, 12.f); break;
        case ACT_FORAGE: a.st = AS_FEED; a.stT = frng.range(4.f, 8.f); break;
        case ACT_LEAVE: {
            const float sx = a.pos.x - cam.x, sy = a.pos.y - cam.y;
            if (!(sx > -80 && sx < LW + 80 && sy > -90 && sy < LH + 90)) a.gone = true;  // home, and out of sight: it is gone
            else { a.st = AS_REST; a.stT = frng.range(15.f, 25.f); }                      // home, but watched: it lies down until you look away
            break;
        }
        case ACT_PATROL: a.idleT = frng.range(1.f, 3.f); break;
        default: a.idleT = frng.range(1.2f, 4.5f); break;
    }
}

// ---- wolf packs: 3-5 wolves with a leader. The followers keep to the leader (they go where it goes and sleep beside it), flank the hunter when
// the pack hunts, break off sooner when hurt, and scatter if the leader falls. A howl at dusk tells the player the pack is out.
Animal* Game::packLeader(int id) {
    for (auto& a : animals)
        if (a.pack == id && a.leader && !a.dead && !a.gone && !a.leaving) return &a;
    return nullptr;
}

void Game::promotePack(int id) {
    if (packLeader(id)) return;
    for (auto& a : animals)
        if (a.pack == id && !a.dead && !a.gone && !a.leaving) { a.leader = true; return; }
}

void Game::spawnPack(int size, float minDist, bool offscreen) {
    if (size <= 1) { trySpawn(A_WOLF, minDist, offscreen); return; }
    if (!trySpawn(A_WOLF, minDist, offscreen)) return;
    const size_t li = animals.size() - 1;
    const int id = nextPack++;
    animals[li].pack = id;
    animals[li].leader = true;
    for (int i = 1; i < size; i++)
        for (int tries = 0; tries < 14; tries++) {
            const V2 q = animals[li].pos + V2(frng.range(-55.f, 55.f), frng.range(-45.f, 45.f));
            if (world.blocked(q.x, q.y, 9) || world.biomeAtPos(q.x, q.y) != B_HOME || world.inTown(q.x, q.y, 48.f)) continue;
            Animal* m = spawnAnimal(A_WOLF, q, animals[li].variant, rollTier());
            m->pack = id;
            m->packSlot = i;
            m->flankSide = i % 2 ? 1.f : -1.f;
            break;
        }
}

// one wolf has seen the hunter: the rest of its pack (within ~500 px, awake or asleep) join the chase
void Game::packAlert(Animal& a) {
    if (a.pack < 0) return;
    for (auto& o : animals) {
        if (&o == &a || o.pack != a.pack || o.dead || o.gone) continue;
        if (o.st != AS_WANDER && o.st != AS_FEED && o.st != AS_REST && o.st != AS_HUNT) continue;
        if (dist(o.pos, a.pos) > 500.f || o.calmT > 0) continue;
        o.st = AS_CHASE;
        o.stT = 12.f;
        o.chasing = false;
    }
}

// a pack member has died: if it was the leader the others scatter for a few seconds (and a new leader is picked)
void Game::packLoses(Animal& d) {
    const int id = d.pack;
    const bool wasLeader = d.leader;
    d.leader = false;
    d.pack = -1;
    if (wasLeader)
        for (auto& o : animals) {
            if (o.pack != id || o.dead || o.gone) continue;
            o.st = AS_FLEE;
            o.fleeAnimal = false;
            o.stT = frng.range(5.f, 8.f);
            o.threat = p.pos;
        }
    promotePack(id);
}

void Game::howl(V2 at, float vol) {
    const float d = dist(at, p.pos);
    howls++;
    audio.play(S_HOWL, vol * clampf(1.25f - d / 1500.f, 0.1f, 1.f));
    const float sx = at.x - cam.x, sy = at.y - cam.y;
    if (sx > -40 && sx < LW + 40 && sy > -40 && sy < LH + 40) ftext(at + V2(0, -22), "AWOOO!", C_GOLD, 1.4f);
}

// How much of its usual notice range an animal keeps against a target, 0..1.3:
//  - its field of view: prey see almost all round (a blind arc behind), predators look further ahead; what is outside it is only heard
//  - cover: each tree, rock or wall between them hides the target (one 60%, two 35%, three or more entirely)
//  - dusk and night blind the prey; a sleeping animal is slow to notice (it is a light sleeper)
//  - noise: a sprinting hunter is heard from far off, a hunter standing still is hard to notice; heard-range is a floor the other factors cannot go below
float Game::sightFactor(const Animal& a, V2 tgt, bool hunterTarget) const {
    const AnimalDef& D = ANIMAL[a.kind];
    const V2 to = tgt - a.pos;
    const float d = to.len();
    if (d < 1.f) return 1.f;
    V2 fv;
    if (a.vel.len() > 2.f) fv = a.vel.norm();
    else fv = a.face8 == F_E ? V2(1, 0) : a.face8 == F_W ? V2(-1, 0) : a.face8 == F_N ? V2(0, -1) : V2(0, 1);
    float f = 1.f;
    if ((fv.x * to.x + fv.y * to.y) / d < (D.aggro ? -0.17f : -0.707f)) f *= 0.35f;  // outside its field of view (100 / 135 degrees each side)
    const int cover = world.coverBetween(a.pos, tgt);
    f *= cover == 0 ? 1.f : cover == 1 ? 0.6f : cover == 2 ? 0.35f : 0.f;
    if (!D.aggro) f *= 1.f - 0.2f * (1.f - daylight());
    if (a.act == ACT_SLEEP && a.st == AS_REST) f *= D.aggro ? 0.7f : 0.6f;
    float hear = D.aggro ? 0.28f : 0.2f;
    if (hunterTarget) {
        if (p.sprinting) { f *= 1.3f; hear = D.aggro ? 0.7f : 0.5f; }
        else if (!p.walking) f *= 0.75f;
    }
    return std::max(f, hear * (a.st == AS_REST && a.act == ACT_SLEEP ? 0.7f : 1.f));
}

// ---- the AI debug overlay (F3): every animal's act and state, its route and bed, and, close to the hunter, its field of view and line of sight
void Game::drawAIDebug() {
    static const char* ACTN[ACT_COUNT] = {"roam", "sleep", "graze", "drink", "forage", "patrol", "leave"};
    static const char* STN[6] = {"wander", "flee", "chase", "feed", "rest", "hunt"};
    auto line = [&](V2 a, V2 b, u32 c) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, cR(c), cG(c), cB(c), cA(c));
        SDL_RenderDrawLine(ren, (int)lroundf(a.x - camX), (int)lroundf(a.y - camY), (int)lroundf(b.x - camX), (int)lroundf(b.y - camY));
    };
    static const char* KN[4] = {"rabbit", "deer", "boar", "wolf"};
    for (const Animal& a : animals) {
        if (a.dead || a.gone) continue;
        const float sx = a.pos.x - camX, sy = a.pos.y - camY;
        if (sx < -40 || sx > LW + 40 || sy < -40 || sy > LH + 40) continue;
        const AnimalDef& D = ANIMAL[a.kind];
        const bool rt = routineAnimal(a);
        if (rt) {
            if (a.hasBed) {  // the bed: a small cross, joined by a faint line
                line(a.pos, a.bed, 0x4060A060);
                line(a.bed + V2(-3, 0), a.bed + V2(3, 0), 0xFF6090FF);
                line(a.bed + V2(0, -3), a.bed + V2(0, 3), 0xFF6090FF);
            }
            if (a.st == AS_WANDER && !a.actArrived) line(a.pos, a.target, 0xB0FFE060);  // where it is walking to
        }
        // what it is up to
        if (a.pack >= 0 && !a.leader)
            if (const Animal* L = packLeader(a.pack)) line(a.pos, L->pos, 0x50FF80FF);  // to its leader
        const std::string label = std::string(a.kind < 4 ? KN[a.kind] : "?") + (a.pack >= 0 ? (a.leader ? "#" + std::to_string(a.pack) + "L" : "#" + std::to_string(a.pack)) : std::string("")) + (rt ? std::string(" ") + ACTN[a.act] : std::string("")) + (a.leaving ? "*" : "") + " " + STN[a.st];
        text((int)sx - (int)label.size() * 2, (int)sy - (int)a.bodyH - 12, label, a.st == AS_CHASE || a.st == AS_HUNT ? C_RED : a.st == AS_FLEE ? C_GOLD : C_TEXT, 1, true);
        // its view, when the hunter is within reach of its notice range: the cone's two edges and the line of sight to the hunter
        const float notice = D.notice * TIER[a.tier].notice * (D.aggro ? D.alertMul : 1.f);
        const float dd = dist(a.pos, p.pos);
        if (dd < notice * 1.6f) {
            V2 fv;
            if (a.vel.len() > 2.f) fv = a.vel.norm();
            else fv = a.face8 == F_E ? V2(1, 0) : a.face8 == F_W ? V2(-1, 0) : a.face8 == F_N ? V2(0, -1) : V2(0, 1);
            const float lim = acosf(D.aggro ? -0.17f : -0.707f), base = atan2f(fv.y, fv.x);
            for (int s = -1; s <= 1; s += 2) line(a.pos, a.pos + V2(cosf(base + s * lim), sinf(base + s * lim)) * notice * 0.5f, 0x6040C0FF);
            const int cover = world.coverBetween(a.pos, p.pos);
            const float f = sightFactor(a, p.pos, true);
            const bool sees = dd < notice * f;
            line(a.pos, p.pos, sees ? 0xC0FF4040 : cover ? 0x80FFA040 : 0x6040FF60);
            text((int)sx - 14, (int)sy + 4, "view " + std::to_string((int)lroundf(f * 100)) + "% cover " + std::to_string(cover), sees ? C_RED : C_DIM, 1, true);
        }
    }
    textC(LW / 2 - 20, 17, "AI DEBUG (F3)", C_GOLD, 1, true);
}

bool Game::trySpawn(int kind, float minDist, bool offscreen) {
    // how close to the village each species may be placed: rabbits live nearest, wolves keep deep in the forest. The far limit is the
    // whole home forest, not a ring around the village.
    static const float nearest[A_COUNT] = {140, 240, 360, 460};
    const float night = 1.f - daylight();
    for (int it = 0; it < 400; it++) {
        const V2 pos(rng.range(4 * TS, (MW - 4) * TS), rng.range(4 * TS, (MH - 4) * TS));
        if (world.biomeAtPos(pos.x, pos.y) != B_HOME) continue;
        if (dist(pos, world.village) < nearest[kind]) continue;
        if (world.blocked(pos.x, pos.y, 9)) continue;
        if (world.inTown(pos.x, pos.y, 48.f)) continue;  // the town is no place for wild animals
        if (dist(pos, p.pos) < minDist) continue;
        if (kind < 4 && !frng.chance(zoneWeight(world, kind, (int)(pos.x / TS), (int)(pos.y / TS), night) / ZONE_MAX[kind])) continue;  // each species keeps to the ground it likes
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
    if (ren) ensureRichBank(ren, A, kind, form, tier);  // a rarer specimen's sprites are drawn the first time one appears
    a.face = frng.chance(0.5f) ? 1 : -1;
    a.bias = frng.chance(0.5f) ? 1.f : -1.f;
    a.idleT = frng.range(0, 3);
    a.animT = frng.range(0, 4);
    a.bodyH = D.bodyH * a.scale;
    if (D.flags & BH_SOARS) a.air = FLY_HEIGHT;  // soaring birds spawn already airborne; the others start on the ground
    animals.push_back(a);
    return &animals.back();
}

static bool nearDeepWater(const World& w, int tx, int ty, int r) {
    for (int y = ty - r; y <= ty + r; y++)
        for (int x = tx - r; x <= tx + r; x++)
            if (w.water(x, y)) return true;
    return false;
}

// How a storm scales a species' local cap: wet-lovers come out in the rain; flyers and big predators stay under cover.
static float rainCapMul(int kind) {
    switch (kind) {
        case A_FROG: case A_DUCK: case A_GATOR: case A_MOCCASIN: return 1.5f;
        case A_EAGLE: case A_WOLF: case A_COYOTE: case A_PANTHER: case A_MTLION: return 0.5f;
        default: return 1.f;
    }
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
        int cap = std::max(1, (int)((S.cap + (night > 0.5f ? (int)S.capNight : 0)) * seasonHerds() + 0.5f));
        cap = (int)ceilf(cap * (1.f + (rainCapMul(S.kind) - 1.f) * rainK()));  // storm: frogs and gators out, wolves and eagles in hiding (live ones stay)
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
    int counts[A_COUNT] = {0};
    for (int k = 0; k < 4; k++) counts[k] = homeTarget(k);  // (only the original four live in the home forest)
    for (int k = 0; k < A_COUNT; k++)
        if (k == A_WOLF) {  // wolves come in packs of 3-5 (the odd one or two left over are lone wolves)
            for (int left = counts[k]; left > 0;) {
                const int size = left >= 3 ? std::min(left, frng.irange(3, 5)) : left;
                spawnPack(size, 110, false);
                left -= size;
            }
        } else
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
        if (world.inTown(np.x, np.y, 32.f) && !world.inTown(a.pos.x, a.pos.y, 32.f)) continue;  // wild animals keep out of the town
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
    // ---- the food chain: predators that are stalking prey this frame, and who counts as their prey
    int hunters[64], nh = 0;
    for (size_t i = 0; i < animals.size() && nh < 64; i++)
        if (!animals[i].dead && !animals[i].gone && animals[i].st == AS_HUNT && ANIMAL[animals[i].kind].preyMask) hunters[nh++] = (int)i;
    // pr is prey of h: same biome, alive, and never a rare or legendary specimen (those are the hunter's trophies)
    auto preyOf = [&](const Animal& pr, const Animal& h) {
        return &pr != &h && !pr.dead && !pr.gone && pr.tier < TR_RARE && pr.biome == h.biome && ((ANIMAL[h.kind].preyMask >> pr.kind) & 1u);
    };
    // v bolts from `from` (a predator), and its herd mates close by bolt with it
    auto fleePredator = [&](Animal& v, V2 from) {
        for (auto& o : animals) {
            if (o.dead || o.gone || o.kind != v.kind || o.biome != v.biome) continue;
            if (&o != &v && (dist(o.pos, v.pos) > 75.f || (o.st != AS_WANDER && o.st != AS_FEED && o.st != AS_REST))) continue;
            o.st = AS_FLEE;
            o.fleeAnimal = true;
            o.threat = from;
            o.stT = frng.range(2.5f, 4.f);
            o.alertT = 0;
        }
    };
    // a startled animal warns the same species close by: they bolt from the hunter too
    auto alarmHerd = [&](Animal& v) {
        for (auto& o : animals) {
            if (&o == &v || o.dead || o.gone || o.kind != v.kind || o.biome != v.biome || dist(o.pos, v.pos) > 70.f) continue;
            if (o.st != AS_WANDER && o.st != AS_FEED && o.st != AS_REST) continue;
            if (ANIMAL[o.kind].aggro) continue;  // fighters do not scatter
            o.st = AS_FLEE;
            o.fleeAnimal = false;
            o.stT = frng.range(2.5f, 4.f);
        }
    };
    {  // the dusk howl: as the evening comes the packs call; and a howl that has just been given is not repeated at once
        const int hr = hourOfDay();
        if (howlCool > 0) howlCool -= dt;
        if (hr == H_DUSK && lastHour != H_DUSK && lastHour >= 0) {
            int n = 0;
            for (auto& a : animals)
                if (a.leader && !a.dead && !a.gone && a.biome == B_HOME && n < 3) { howl(a.pos, 0.6f); n++; }
        }
        lastHour = hr;
    }
    for (auto& a : animals) {
        if (a.st == AS_REST && a.act == ACT_SLEEP && routineAnimal(a) && a.hp < a.maxhp && !a.dead) {  // asleep at its bed: slowly heals
            a.regenAcc += dt * a.maxhp * 0.012f;
            if (a.regenAcc >= 1.f) { const int h = (int)a.regenAcc; a.hp = std::min(a.maxhp, a.hp + h); a.regenAcc -= h; }
        }
        if (a.st != AS_FLEE) a.fleeAnimal = false;
        if (a.huntCd > 0) a.huntCd -= dt;
        if (a.flash > 0) a.flash -= dt;
        if (a.hpShow > 0) a.hpShow -= dt;
        if (a.atkCd > 0) a.atkCd -= dt;
        // knockback dies away fast on a living animal; a dying one keeps its momentum and slides to a stop (friction, not a cut-off)
        if (a.kb.len() > (a.dead ? 2.f : 3.f)) {
            const V2 from = a.pos;
            moveEnt(world, a.pos, a.kb * dt, 5.f);
            if (a.dead && a.kb.len() > 25.f && rng.chance(dt * 7.f)) decals.push_back({a.pos + V2(rng.range(-2, 2), 2), 0, 0, 40});  // a smear along the slide
            if (a.dead && dist(from, a.pos) < a.kb.len() * dt * 0.3f) a.kb = a.kb * 0.5f;  // ran into something: scrub the speed off
            a.kb = a.kb * expf((a.dead ? -3.4f : -9.f) * dt);
        } else
            a.kb = V2();
        if (a.dead) {
            a.deadT += dt;
            updateAnimalAnim(a, dt);
            if (a.deadT > (a.tier == TR_LEGENDARY ? CORPSE_LIFE_LEGEND : CORPSE_LIFE)) a.fade -= dt / 8.f;
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
        if (a.calmT > 0) a.calmT -= dt;
        if (a.st != AS_CHASE) a.chasing = false;
        a.vel = V2();
        if (a.alertT > 0) {  // startled: freeze and look at the hunter, then bolt (the state change was made when it noticed)
            a.alertT -= dt;
            if (dd < 9000.f) a.face8 = facingFromDir(dp.x, dp.y, a.face8);
            updateAnimalAnim(a, dt);
            continue;
        }
        switch (a.st) {
            case AS_WANDER:
            case AS_FEED:
            case AS_REST: {
                bool alert = false;
                if (a.calmT <= 0) {  // it notices the hunter inside its notice range, as far as its view allows (cone, cover, noise: sightFactor)
                    const float reach = D.aggro ? notice * D.alertMul : notice;
                    alert = dd < reach && dd < reach * sightFactor(a, p.pos, true);
                }
                if (!alert)
                    for (int hi = 0; hi < nh; hi++) {  // prey notice a predator that is hunting close by (if they can see it)
                        const Animal& h = animals[hunters[hi]];
                        const float hd = dist(a.pos, h.pos);
                        if (preyOf(a, h) && hd < D.notice * T.notice * 0.9f && hd < D.notice * T.notice * 0.9f * sightFactor(a, h.pos, false)) {
                            fleePredator(a, h.pos);
                            break;
                        }
                    }
                if (a.st == AS_FLEE) break;
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
                        if (a.pack >= 0) {  // the pack joins in (and the one that saw you gives a howl)
                            packAlert(a);
                            if (howlCool <= 0) { howl(a.pos, 0.55f); howlCool = 14.f; }
                        }
                    } else {
                        a.st = AS_FLEE;
                        a.stT = rng.range(2.5f, 4.f);
                        alarmHerd(a);
                        if (const AnimalClips* ac = animalClips(a.kind))
                            if (ac->poses[AN_ALERT] > 0) a.alertT = ac->clip[AN_ALERT].total();  // a moment of alert before it bolts
                        ftext(a.pos + V2(0, -a.bodyH - 12), "!", C_TEXT, 0.7f);
                    }
                    break;
                }
                if (a.st == AS_WANDER && D.preyMask && a.huntCd <= 0) {  // a predator that sees prey may stalk it
                    bool sees = false;
                    for (auto& pr : animals)
                        if (preyOf(pr, a) && dist(pr.pos, a.pos) < D.notice * T.notice * 0.9f * sightFactor(a, pr.pos, false)) {
                            sees = true;
                            break;
                        }
                    if (sees && frng.chance(dt * 0.6f)) {
                        a.st = AS_HUNT;
                        a.stT = 10.f;
                        break;
                    }
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
                    if (routineAnimal(a)) {  // a daily routine: walk to the place of the act, do it, then choose the next one
                        if (to.len() < 6.f || a.stT < -18.f || (!stepAnimal(a, to.norm(), D.wander * T.speed * (a.act == ACT_PATROL || a.act == ACT_LEAVE ? 1.25f : 1.f), dt) && a.stT < -1.f)) {
                            if (!a.actArrived) performAct(a);
                            else chooseAct(a);
                            a.stT = 0;
                        }
                    } else if (to.len() < 5 || a.stT < -7) {
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
                V2 src = p.pos;
                bool hunted = false;  // a predator is still close (when fleeing from one)
                if (a.fleeAnimal) {
                    float best = D.notice * T.notice * 1.5f;
                    src = a.threat;
                    for (int hi = 0; hi < nh; hi++) {
                        const Animal& h = animals[hunters[hi]];
                        const float d = dist(a.pos, h.pos);
                        if (preyOf(a, h) && d < best) best = d, src = h.pos, hunted = true;
                    }
                    a.threat = src;
                }
                V2 away = (a.pos - src).norm();
                if (away.len() < 0.1f) away = V2(1, 0);
                V2 perp(-away.y, away.x);
                float j = sinf(timeAcc * 2.3f + a.animT) * 0.35f;
                V2 dir = (away + perp * j).norm();
                stepAnimal(a, dir, D.run * T.speed * (a.hp < a.maxhp ? 1.08f : 1.f), dt);
                if (a.stT <= 0 && (a.fleeAnimal ? !hunted : dd > notice * 1.4f)) {
                    a.st = AS_WANDER;
                    a.idleT = 1;
                    a.target = a.pos;
                a.actArrived = true;  // (a routine animal chooses afresh)
                }
                break;
            }
            case AS_HUNT: {
                if (dd < (D.aggro ? notice * D.alertMul : notice) * (dd < 9000.f ? sightFactor(a, p.pos, true) : 1.f)) {  // the hunter outranks any prey
                    a.st = agg ? AS_CHASE : AS_FLEE;
                    a.stT = agg ? 12.f : rng.range(2.5f, 4.f);
                    a.fleeAnimal = false;
                    break;
                }
                int best = -1;
                float bd = 220.f;
                for (size_t j = 0; j < animals.size(); j++) {
                    if (!preyOf(animals[j], a)) continue;
                    const float d = dist(animals[j].pos, a.pos);
                    if (d < bd) bd = d, best = (int)j;
                }
                if (best < 0 || a.stT <= 0) {  // nothing left to chase, or out of breath: rest before the next hunt
                    a.st = AS_WANDER;
                    a.huntCd = frng.range(30.f, 70.f);
                    a.idleT = 1.5f;
                    a.target = a.pos;
                a.actArrived = true;  // (a routine animal chooses afresh)
                    break;
                }
                Animal& pr = animals[best];
                const V2 toPrey = (pr.pos - a.pos).norm();
                if (bd < D.hitR + 8.f) {
                    if (a.atkCd <= 0) {  // a bite
                        pr.hp -= std::max(1, (int)lroundf(D.dmg * T.dmg * 0.7f));
                        pr.flash = 0.16f;
                        pr.kb += toPrey * 60.f;
                        pr.alertT = 0;
                        bloodBurst(pr.pos + V2(0, -pr.bodyH), toPrey, 5);
                        if (pr.hp > 0) {
                            pr.bleedT = BLEED_TIME;
                            pr.bleedByHunter = false;
                            if (pr.bleedTick <= 0) pr.bleedTick = 1.f;
                        }
                        if (const AnimalClips* ac = animalClips(a.kind)) a.atkT = ac->clip[AN_ATTACK].total();
                        if (const AnimalClips* pc = animalClips(pr.kind)) {
                            pr.hurtT = pc->clip[AN_HURT].total();
                            pr.anim.play(AN_HURT, &pc->clip[AN_HURT], true);
                        }
                        a.atkCd = D.atkCd * 1.2f;
                        a.face8 = facingFromDir(toPrey.x, toPrey.y, a.face8);
                        if (pr.hp <= 0) {  // the kill: the carcass stays for the hunter to find; the predator feeds and then rests
                            killAnimal(pr, false);
                            a.st = AS_FEED;
                            a.stT = frng.range(6.f, 10.f);
                            a.huntCd = frng.range(50.f, 90.f);
                        } else
                            fleePredator(pr, a.pos);
                    }
                } else
                    stepAnimal(a, toPrey, D.run * T.speed * 0.92f, dt);
                break;
            }
            case AS_CHASE: {
                V2 toP = dp.norm();
                if (!a.chasing) {
                    a.chasing = true;
                    a.chaseFrom = a.pos;
                }
                // a chase has limits: it ends when its timer runs out or CHASE_RANGE pixels from where it began, and the animal then
                // loses interest for CHASE_COOLDOWN seconds
                if (a.stT <= 0 || dist(a.pos, a.chaseFrom) > CHASE_RANGE) {
                    a.st = AS_WANDER;
                    a.chasing = false;
                    a.calmT = CHASE_COOLDOWN;
                    a.idleT = 1.5f;
                    a.target = a.pos;
                a.actArrived = true;  // (a routine animal chooses afresh)
                    break;
                }
                if (std::max(D.fleeHp, a.pack >= 0 ? 0.4f : 0.f) > 0 && a.hp < a.maxhp * std::max(D.fleeHp, a.pack >= 0 ? 0.4f : 0.f)) {  // (a pack wolf breaks off sooner)
                    a.st = AS_FLEE;
                    a.stT = 6;
                    break;
                }
                if (dd > D.leash || p.dead || !agg) {
                    a.st = AS_WANDER;
                    a.idleT = 1.5f;
                    a.target = a.pos;
                a.actArrived = true;  // (a routine animal chooses afresh)
                    break;
                }
                a.face = dp.x > 0 ? 1 : -1;
                if (dd < D.hitR + 9) {
                    if (a.atkCd <= 0) {
                        hurtPlayer(D.dmg * T.dmg, a.pos, a.kind);
                        if (const AnimalClips* ac = animalClips(a.kind)) a.atkT = ac->clip[AN_ATTACK].total();
                        a.atkCd = D.atkCd;
                        a.kb = toP * -70;
                    }
                } else {
                    V2 goal = toP;
                    if (a.pack >= 0 && !a.leader && dd > 95.f && dd < 380.f) {  // flanking: it aims for a point to one side of the hunter, so the pack closes from several directions
                        const V2 side(-toP.y, toP.x);
                        goal = (p.pos + side * (a.flankSide * 85.f) - a.pos).norm();
                    }
                    stepAnimal(a, goal, D.run * T.speed * (dd < 50 ? 1.12f : 1.f), dt);
                }
                break;
            }
        }
        if (a.bleedT > 0) {  // a wounded animal drips blood as it moves
            a.bleedT -= dt;
            a.bleedTick -= dt;
            if (a.bleedTick <= 0 && a.hp > 0) {  // the wound costs health
                a.bleedTick = 1.f;
                const int loss = std::max(1, (int)lroundf(a.maxhp * BLEED_FRAC));
                a.hp -= loss;
                a.hpShow = 2.f;
                ftext(a.pos + V2(0, -a.bodyH - 10), "-" + S(loss), HEX(0xb52a2e), 0.7f);
                if (a.hp <= 0) {
                    if (a.bleedByHunter && dist(a.pos, p.pos) > 150.f) {  // the quarry got away wounded and bled out: say where to find it
                        static const char* dirs[8] = {"EAST", "SOUTH-EAST", "SOUTH", "SOUTH-WEST", "WEST", "NORTH-WEST", "NORTH", "NORTH-EAST"};
                        const V2 d = a.pos - p.pos;
                        const int di = ((int)lroundf(atan2f(d.y, d.x) / (3.14159265f / 4.f)) + 8) % 8;
                        a.tracked = true;
                        msg("YOUR WOUNDED " + std::string(ANIMAL[a.kind].name) + " BLED OUT " + S((int)(d.len() / TS)) + " TILES " + dirs[di], C_GOLD);
                    }
                    killAnimal(a, a.bleedByHunter);
                    continue;
                }
            }
            if (a.vel.len() > 5.f && rng.chance(dt * 6.f)) {
                decals.push_back({a.pos + V2(rng.range(-2, 2), 2), 0, 0, 70});  // the trail stays long enough to follow
                bloodBurst(a.pos + V2(0, -a.bodyH * 0.5f), V2(0, 1), 1);
            }
        }
        if (fabsf(a.vel.x) > 2.f && a.st != AS_CHASE) a.face = a.vel.x > 0 ? 1 : -1;
        else if (fabsf(a.vel.x) > 2.f) a.face = a.vel.x > 0 ? 1 : -1;
        float spd = a.vel.len();
        if (spd > 1) a.animT += spd * dt * D.animK;
        a.noticeT += dt;
        const int prevAnim = a.anim.state;
        updateAnimalAnim(a, dt);
        if (animalClips(a.kind) && a.air < 1.f) {  // dust kicked up by a running animal, and a puff as a bird launches itself
            if (a.anim.state == AN_RUN && rng.chance(dt * 9.f)) puff(a.pos + V2(-a.vel.x * 0.04f, -1), 1, HEX(0xcdb98f));
            if (a.anim.state == AN_TAKEOFF && prevAnim != AN_TAKEOFF) puff(a.pos + V2(0, -1), 4, HEX(0xd8c8a8));
        }
    }
    animals.erase(std::remove_if(animals.begin(), animals.end(), [](const Animal& a) { return a.gone; }), animals.end());
}

// Choose and advance the animation of an animal that has a full set (animal_anim.hpp). The AI decides what the animal does; this
// only reads it: death, a fresh hit, a bite landing, running (fleeing / charging), walking, idling. Walk and run play at the pace
// of the animal's real speed so the feet do not slide.
void Game::updateAnimalAnim(Animal& a, float dt) {
    const AnimalClips* ac = animalClips(a.kind);
    if (!ac) return;
    const AnimalDef& D = ANIMAL[a.kind];
    const float spd = a.vel.len();
    a.hurtT = std::max(0.f, a.hurtT - dt);
    a.atkT = std::max(0.f, a.atkT - dt);
    int st;
    float rate = 1.f;
    // flyers climb to FLY_HEIGHT while fleeing (soarers while moving about) and come down to rest or feed: air eases between the two
    int flight = 0;  // 1 climbing, 2 cruising, 3 descending
    if ((D.flags & BH_FLIES) && !a.dead && a.alertT <= 0) {
        const bool soar = (D.flags & BH_SOARS) && a.st != AS_REST && a.st != AS_FEED && (spd > 1.f || a.st == AS_CHASE);
        const float goal = (a.st == AS_FLEE || soar) ? FLY_HEIGHT : 0.f;
        if (a.air < goal) a.air = std::min(goal, a.air + FLY_CLIMB * dt);
        else if (a.air > goal) a.air = std::max(goal, a.air - FLY_DESCEND * dt);
        flight = a.air < goal ? 1 : (a.air > goal ? 3 : (a.air > 0.5f ? 2 : 0));
    }
    if (a.dead) st = AN_DEATH;
    else if (a.hurtT > 0) st = AN_HURT;
    else if (a.atkT > 0) st = AN_ATTACK;
    else if (a.alertT > 0) st = AN_ALERT;
    else if (flight == 1) st = (a.anim.state == AN_TAKEOFF && a.anim.done) ? AN_FLY : AN_TAKEOFF;
    else if (flight == 2) st = AN_FLY;
    else if (flight == 3) st = AN_LAND;  // holds its last pose until the feet touch down
    else if (spd > 1.f) {
        const bool fast = a.st == AS_CHASE || a.st == AS_FLEE || a.st == AS_HUNT;
        st = fast ? AN_RUN : AN_WALK;
        rate = clampf(spd / (fast ? D.run : D.wander), 0.5f, 1.8f);
    } else if (a.st == AS_FEED && ac->poses[AN_EAT] > 0) st = AN_EAT;
    else if (a.st == AS_REST && ac->poses[AN_REST] > 0) st = AN_REST;
    else st = AN_IDLE;
    if (!a.dead) {
        if (a.st == AS_CHASE && dist(a.pos, p.pos) < D.hitR + 30) a.face8 = facingFromDir(p.pos.x - a.pos.x, p.pos.y - a.pos.y, a.face8);
        else if (spd > 1.f) a.face8 = facingFromDir(a.vel.x, a.vel.y, a.face8);
    }
    a.anim.play(st, &ac->clip[st]);
    a.anim.update(dt * rate);
}

void Game::hitAnimal(Animal& a, float dmg, V2 dir, bool crit) {
    int d = std::max(1, (int)roundf(dmg));
    a.hp -= d;
    a.alertT = 0;  // a hit ends any startle: it bolts at once
    a.flash = 0.16f;
    a.hpShow = 3.0f;
    if (const AnimalClips* ac = animalClips(a.kind)) {
        a.hurtT = ac->clip[AN_HURT].total();
        a.atkT = 0;
        a.anim.play(AN_HURT, &ac->clip[AN_HURT], true);  // every hit restarts the flinch
    }
    a.kb += dir * (crit ? 150.f : 105.f);
    bloodBurst(a.pos + V2(0, -a.bodyH), dir, crit ? 14 : 8);
    if (a.hp > 0) {
        a.bleedT = crit ? BLEED_TIME * 1.5f : BLEED_TIME;
        a.bleedByHunter = true;
        if (a.bleedTick <= 0) a.bleedTick = 1.f;
    }
    ftext(a.pos + V2(0, -a.bodyH - 10), (crit ? std::string("CRIT ") : std::string()) + S(d), crit ? C_ORANGE : C_TEXT, 0.9f);
    audio.play(crit ? S_CRIT : S_HIT);
    shakeAmt = std::max(shakeAmt, crit ? 3.f : 1.5f);
    decals.push_back({a.pos + V2(rng.range(-5, 5), rng.range(0, 4)), 0, 0, 40});
    const AnimalDef& D = ANIMAL[a.kind];
    bool agg = D.aggro && (!(D.flags & BH_NIGHT_AGGRO) || daylight() < 0.55f);
    if (agg) {
        a.st = AS_CHASE;
        a.stT = 10;
        a.calmT = 0;  // being hit overrides the cool-down
    } else {
        a.st = AS_FLEE;
        a.stT = 7;
        a.fleeAnimal = false;  // it flees from the hunter now
    }
    if (a.hp <= 0) killAnimal(a);
}

void Game::killAnimal(Animal& a, bool byHunter) {
    if (a.pack >= 0) packLoses(a);
    a.dead = true;
    a.deadT = 0;
    a.hp = 0;
    {  // the death slide: the killing blow's push plus what is left of the animal's own run carries the body along
        const V2 carry = a.vel * 0.7f;
        V2 m = a.kb + carry;
        const float ml = m.len();
        if (ml > 1e-3f) a.kb = m * (std::min(std::max(ml, 80.f), 190.f) / ml);
    }
    a.vel = V2();
    if (byHunter) {
        kills++;
        bestiaryKill(a.kind, a.tier);
        companionKill();
        questEvent(Q_KILL, a.kind, 1);
    }
    Rng& dropRng = byHunter ? rng : frng;  // a predator's kill must not disturb the original random sequence
    const AnimalDef& D = ANIMAL[a.kind];
    int total = 0;
    for (int m = 0; m < M_COUNT; m++) {
        a.remain[m] = D.drop[m][1] > 0 ? dropRng.irange(D.drop[m][0], D.drop[m][1]) : 0;
        // winter coats are thick, autumn game is fat
        if (a.remain[m] > 0 && ((season() == SEASON_WINTER && (m == M_FUR || m == M_HIDE)) || (season() == SEASON_AUTUMN && (m == M_MEAT || m == M_FAT)))) a.remain[m]++;
        total += a.remain[m];
    }
    if (D.formDrop[0] >= 0 && a.variant == 1) {  // e.g. a buck's antlers
        int n = dropRng.irange(D.formDrop[1], D.formDrop[2]);
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
        if (byHunter && TIER[a.tier].bounty > 0) {
            p.coins += TIER[a.tier].bounty;
            ftext(a.pos + V2(0, -a.bodyH - 30), "+" + S(TIER[a.tier].bounty) + "C", C_GOLD, 1.6f);
        }
        if (byHunter) msg(std::string(TIER[a.tier].name) + " " + animalName(a.kind, a.tier) + " HUNTED!", TIER[a.tier].col);
    }
    a.air = 0;
    if (total == 0) {
        a.remain[M_MEAT] = 1;
        total = 1;
    }
    a.totalRemain = total;
    decals.push_back({a.pos + V2(0, 3), D.decal, 0, 80});
    bloodBurst(a.pos + V2(0, -a.bodyH), a.kb.norm(), 16);
    if (!byHunter) return;  // killed by a predator: the carcass is there to find, but there is no kill credit, fanfare or XP
    if (frng.range(0.f, 1.f) < babyChance(a.kind, a.tier)) {  // a young one was nearby: it can be raised at the homestead
        GroundItem g{a.pos + V2(frng.range(-10, 10), 6), 0, 0.f, 0.f};
        g.item = itemBaby(a.kind);
        g.n = 1;
        g.q = a.tier;
        items.push_back(g);
        msg("A BABY " + std::string(ANIMAL[a.kind].name) + " WAS NEARBY!", C_GREEN);
    }
    audio.play(S_KILL);
    shakeAmt = std::max(shakeAmt, 4.f);
    ftext(a.pos + V2(0, -a.bodyH - 18), "HUNTED!", C_GOLD, 1.4f);
    // skill XP for a completed kill (every kill is made with a thrown weapon or a launcher); bigger and rarer game pays more
    const float tierMul = a.tier > 0 ? TIER[a.tier].drops : 1.f;
    const int rec = 5 + (int)(D.hp / 3);  // level at which this animal is no longer a challenge
    grantXp(SK_HUNTING, std::max(1, (int)lroundf((3 + D.hp / 6.f) * tierMul * xpFalloff(p.sk.level(SK_HUNTING), rec))));
    grantXp(SK_RANGED, std::max(1, (int)lroundf((2 + D.hp / 8.f) * tierMul * xpFalloff(p.sk.level(SK_RANGED), rec))));
}
