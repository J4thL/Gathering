// selftest_snow.cpp - `--selftest`: the snowfields' wildlife: the seal, its breathing holes, the dive, and what a blizzard does to the animals.
#include "tests/selftest_ctx.hpp"
#include "world/light_vis.hpp"

void testSnowLife(SelfTest& t) {
    Game& g = t.g;
    auto check = [&](bool ok, const char* what) { t.check(ok, what); };
    printf("snow wildlife\n");
    const int map0 = g.world.mapId;
    const V2 pos0 = g.p.pos;
    const int wx0 = g.wxKind, interior0 = g.interior, delve0 = g.delve;
    const float storm0 = g.stormK;
    g.loadMapWorld(B_SNOW);
    g.animals.clear();
    // the breathing holes: only on the ice, and a few of them
    std::vector<V2> holes;
    for (const Decor& d : g.world.decor)
        if (d.kind == D_ICEHOLE) holes.push_back(V2(d.x, d.y));
    bool onIce = !holes.empty();
    for (const V2& h : holes) onIce = onIce && g.world.tile[(int)(h.y / TS)][(int)(h.x / TS)] == T_ICE;
    check(holes.size() >= 3 && onIce, "the frozen lakes have a few breathing holes, all on the ice");
    if (holes.empty()) {
        g.loadMapWorld(map0);
        return;
    }
    // a hunter standing a spawn-ring away from a hole: seals appear by the holes in the ring, on the ice and close to a hole
    const V2 hole = holes[holes.size() / 2];
    g.interior = -1;
    g.delve = -1;
    g.p.dead = false;
    g.p.pos = V2(std::max(40.f, std::min(MW * (float)TS - 40.f, hole.x + (hole.x < MW * TS * 0.5f ? 380.f : -380.f))), hole.y);
    g.state = ST_PLAY;
    bool spawned = false, nearHole = true, onIce2 = true;
    for (int i = 0; i < 400 && !spawned; i++) {
        for (int k = 0; k < A_COUNT; k++) g.spCool[k] = 0.f;
        g.spawnT = 0.f;
        g.streamSpawns(0.5f);
        for (const Animal& a : g.animals)
            if (a.kind == A_SEAL && !a.dead) {
                spawned = true;
                float best = 1e9f;
                for (const V2& h : holes) best = std::min(best, dist(h, a.pos));
                nearHole = best < 40.f;
                onIce2 = g.world.tile[(int)(a.pos.y / TS)][(int)(a.pos.x / TS)] == T_ICE;
            }
    }
    check(spawned, "a hunter by the frozen lakes sees seals spawn");
    check(spawned && nearHole && onIce2, "a seal lies on the ice a few steps from a breathing hole");
    // sight: a blizzard blinds the animals
    Animal* seal = nullptr;
    for (Animal& a : g.animals)
        if (a.kind == A_SEAL && !a.dead) seal = &a;
    if (seal) {
        const V2 tgt = seal->pos + V2(40.f, 0.f);
        seal->face8 = F_E;
        seal->vel = V2(0, 0);
        g.wxKind = WX_CLEAR;
        g.stormK = 0.f;
        const float clear = g.sightFactor(*seal, tgt, true);
        g.wxKind = WX_BLIZZARD;
        g.stormK = 1.f;
        const float blizzard = g.sightFactor(*seal, tgt, true);
        check(blizzard < clear * 0.75f && blizzard > 0.f, "in a blizzard an animal sees the hunter much less well");
        g.wxKind = WX_CLEAR;
        g.stormK = 0.f;
        // the dive: a startled seal runs for a hole and slips down it
        g.animals.erase(std::remove_if(g.animals.begin(), g.animals.end(), [&](const Animal& a) { return a.kind != A_SEAL; }), g.animals.end());
        for (size_t i = 1; i < g.animals.size(); i++) g.animals[i].gone = true;
        Animal& s = g.animals[0];
        float best = 1e9f;
        V2 nearest = s.pos;
        for (const V2& h : holes)
            if (dist(h, s.pos) < best) best = dist(h, s.pos), nearest = h;
        // the hunter right beside it, running, on the side away from the hole
        const V2 away = (s.pos - nearest).len() > 1.f ? (s.pos - nearest).norm() : V2(1, 0);
        g.p.pos = s.pos + away * 35.f;
        g.p.sprinting = true;
        s.st = AS_FLEE;
        s.stT = 6.f;
        bool dived = false;
        for (int i = 0; i < 600 && !dived; i++) {
            g.p.pos = s.pos + away * 35.f;  // keeps pushing it
            g.updateAnimals(0.05f);
            dived = s.gone;
        }
        check(dived, "a startled seal slips down its breathing hole");
        check(dived && g.ripples.size() == 3 && g.parts.size() >= 10, "the dive leaves a splash and rings on the water");
        for (int i = 0; i < 40; i++) g.updateFx(0.1f);
        check(g.ripples.empty(), "the rings fade away");
        g.p.sprinting = false;
    }
    // reindeer herds: a leader and others that keep to it, rest and graze with it, and bolt together
    V2 openSnow(0.f, 0.f);
    bool haveSnow = false;
    {
        g.animals.clear();
        V2 spot(0, 0);
        bool found = false;
        for (int ty = 40; ty < MH - 40 && !found; ty += 3)
            for (int tx = 40; tx < MW - 40 && !found; tx += 3) {
                bool ok = true;
                for (int dy = -4; dy <= 4 && ok; dy++)
                    for (int dx = -4; dx <= 4; dx++) {
                        const uint8_t tt = g.world.tile[ty + dy][tx + dx];
                        if ((tt != T_SNOW && tt != T_SNOWP) || g.world.blocked((tx + dx) * (float)TS + 8.f, (ty + dy) * (float)TS + 8.f, 9)) { ok = false; break; }
                    }
                if (ok) found = true, spot = V2(tx * (float)TS + 8.f, ty * (float)TS + 8.f);
            }
        check(found, "there is open snow to put a herd on");
        if (found) openSnow = spot, haveSnow = true;
        if (found) {
            g.p.pos = spot + V2(380.f, 0.f);
            g.p.sprinting = false;
            const bool ok = g.spawnHerd(A_REINDEER, spot, 0, 4);
            int members = 0, leaders = 0;
            int pack = -1;
            for (const Animal& a : g.animals)
                if (a.kind == A_REINDEER) members++, leaders += a.leader, pack = a.pack;
            bool samePack = true;
            for (const Animal& a : g.animals) samePack = samePack && a.pack == pack && pack >= 0;
            check(ok && members >= 3 && leaders == 1 && samePack, "a herd is a leader and 2-3 others in one pack");
            for (int i = 0; i < 1200; i++) {  // 60 seconds of grazing and wandering, the hunter well away
                g.p.pos = spot + V2(380.f, 0.f) + (g.animals.empty() ? V2(0, 0) : V2(g.animals[0].pos.x - spot.x, g.animals[0].pos.y - spot.y));
                g.updateAnimals(0.05f);
            }
            const Animal* L = nullptr;
            for (const Animal& a : g.animals)
                if (a.leader && !a.dead && !a.gone) L = &a;
            float far = 0.f;
            int alive = 0;
            for (const Animal& a : g.animals)
                if (a.kind == A_REINDEER && !a.dead && !a.gone && L) alive++, far = std::max(far, dist(a.pos, L->pos));
            check(L && alive == members && far < 200.f, "the herd keeps together: every member stays near its leader");
            // startle one member: the whole herd bolts
            const Animal* fm = nullptr;
            for (const Animal& a : g.animals)
                if (a.kind == A_REINDEER && !a.leader && !a.dead && !a.gone) fm = &a;
            if (fm) {
                const V2 at = fm->pos;
                g.p.sprinting = true;
                bool allFlee = false;
                for (int i = 0; i < 60 && !allFlee; i++) {
                    g.p.pos = at + V2(30.f, 0.f);
                    g.updateAnimals(0.05f);
                    int fleeing = 0;
                    for (const Animal& a : g.animals)
                        if (a.kind == A_REINDEER && !a.dead && !a.gone && a.st == AS_FLEE) fleeing++;
                    allFlee = fleeing >= alive;
                }
                check(allFlee, "one startled reindeer sends the whole herd running");
                g.p.sprinting = false;
            }
        }
        g.animals.clear();
    }
    // dens and beds: the foxes' dens, and the daily routines of the fox, the hare and the reindeer
    {
        g.animals.clear();
        g.interior = -1;
        g.delve = -1;
        g.wxKind = WX_CLEAR;
        g.stormK = 0.f;
        std::vector<V2> dens;
        for (const Decor& d : g.world.decor)
            if (d.kind == D_DEN) dens.push_back(V2(d.x, d.y));
        bool denSnow = !dens.empty();
        for (const V2& d : dens) {
            const uint8_t tt = g.world.tile[(int)(d.y / TS)][(int)(d.x / TS)];
            denSnow = denSnow && (tt == T_SNOW || tt == T_SNOWP);
        }
        check(dens.size() >= 4 && denSnow, "the snowfields have a few fox dens, all on snow");
        if (dens.size() >= 4) {
            // a den the fox can walk to in a straight line from where it is put (the foxes steer, they do not path-find, and some dens are tucked in among trees)
            V2 den = dens[dens.size() / 2];
            for (size_t k = 0; k < dens.size(); k++) {
                const V2 c = dens[(dens.size() / 2 + k) % dens.size()];
                bool clear = true;
                for (int i = 0; i <= 24 && clear; i++) {
                    const V2 q = c + V2(70.f, 40.f) * (1.f - i / 24.f) + V2(0.f, 3.f) * (i / 24.f);
                    clear = !g.world.blocked(q.x, q.y, 6.f);
                }
                if (clear) { den = c; break; }
            }
            g.p.pos = den + V2(420.f, 0.f);
            g.p.sprinting = false;
            Animal* fox = g.spawnAnimal(A_ARCTICFOX, den + V2(70.f, 40.f), 0, TR_COMMON);
            check(fox != nullptr, "a fox spawns in the snowfields");
            if (fox) {
                fox->hasBed = false;
                const bool got = g.assignBed(*fox);
                float nearest = 1e9f;
                for (const V2& d : dens) nearest = std::min(nearest, dist(d, fox->bed));
                check(got && nearest < 8.f, "a fox takes a den for its bed");
                fox->bed = den + V2(0.f, 3.f);  // the walk below is to this den, the one with a clear way
                // by day a fox mostly sleeps, and it walks to the den and lies down in it
                g.dayT = DAY_LEN * 0.2f;
                int sleeps = 0;
                for (int i = 0; i < 400; i++) {
                    g.chooseAct(*fox);
                    sleeps += fox->act == ACT_SLEEP;
                }
                check(sleeps > 400 * 0.5f, "by day an arctic fox mostly sleeps");
                for (int i = 0; i < 100 && fox->act != ACT_SLEEP; i++) g.chooseAct(*fox);
                fox->st = AS_WANDER;
                fox->actArrived = false;
                for (int i = 0; i < 2000 && !(fox->st == AS_REST && fox->act == ACT_SLEEP); i++) {
                    g.p.pos = den + V2(420.f, 0.f);
                    g.updateAnimals(0.05f);
                }
                check(fox->st == AS_REST && fox->act == ACT_SLEEP && dist(fox->pos, fox->bed) < 16.f, "the fox lies down in its den");
                // a blizzard sends everything to its bed
                g.wxKind = WX_BLIZZARD;
                g.stormK = 1.f;
                bool allSleep = true;
                for (int i = 0; i < 60; i++) {
                    Animal* h = g.spawnAnimal(A_SNOWHARE, den + V2(100.f, 60.f), 0, TR_COMMON);
                    if (h) {
                        g.chooseAct(*h);
                        allSleep = allSleep && h->act == ACT_SLEEP;
                    }
                }
                check(allSleep, "in a blizzard the hares go to their beds");
                g.wxKind = WX_CLEAR;
                g.stormK = 0.f;
            }
            // at night the reindeer bed down
            g.animals.clear();
            g.dayT = DAY_LEN * 0.7f;
            bool nightSleep = true;
            for (int i = 0; i < 30; i++) {
                Animal* r = g.spawnAnimal(A_REINDEER, den + V2(200.f, 80.f), 0, TR_COMMON);
                if (r) {
                    g.chooseAct(*r);
                    nightSleep = nightSleep && r->act == ACT_SLEEP;
                }
            }
            check(nightSleep, "at night the reindeer bed down");
            g.animals.clear();
        }
    }
    // wind and scent: the wind turns, and an animal smells a hunter upwind of it from behind, but not one downwind
    {
        g.animals.clear();
        g.interior = -1;
        g.delve = -1;
        g.wxKind = WX_CLEAR;
        g.stormK = 0.f;
        float lo = 9.f, hi = -9.f;
        bool unit = true;
        const float time0 = g.timeAcc;
        for (int i = 0; i < 60; i++) {
            g.timeAcc = i * 29.f;
            g.updateWeather(0.016f);
            lo = std::min(lo, g.windAng);
            hi = std::max(hi, g.windAng);
            unit = unit && fabsf(g.windDir.len() - 1.f) < 0.01f;
        }
        g.timeAcc = time0;
        check(unit && hi - lo > 1.5f, "the wind turns over time");
        if (haveSnow) {
            g.p.pos = openSnow + V2(420.f, 0.f);
            g.p.sprinting = false;
            g.p.walking = false;
            Animal* r = g.spawnAnimal(A_REINDEER, openSnow, 0, TR_COMMON);
            if (r) {
                r->vel = V2(0.f, 0.f);
                r->st = AS_WANDER;
                r->act = ACT_ROAM;
                g.dayT = DAY_LEN * 0.2f;
                g.windDir = V2(1.f, 0.f);  // blowing east
                g.windAng = 0.f;
                g.wind = 1.f;
                r->face8 = F_E;  // facing east, the hunter behind it to the west: upwind of the animal, outside its view
                const float up = g.sightFactor(*r, r->pos + V2(-60.f, 0.f), true);
                r->face8 = F_W;  // facing west, the hunter behind it to the east: downwind
                const float down = g.sightFactor(*r, r->pos + V2(60.f, 0.f), true);
                g.wind = 0.f;  // no wind: nothing carries
                r->face8 = F_E;
                const float calm = g.sightFactor(*r, r->pos + V2(-60.f, 0.f), true);
                g.wind = 1.f;
                g.stormK = 1.f;
                g.wxKind = WX_STORM;  // heavy rain washes the scent out
                const float rain = g.sightFactor(*r, r->pos + V2(-60.f, 0.f), true);
                g.wxKind = WX_CLEAR;
                g.stormK = 0.f;
                check(up >= 0.5f, "an animal smells a hunter upwind of it, even from behind");
                check(down < 0.4f, "an animal does not smell a hunter downwind of it");
                check(calm < 0.4f, "with no wind there is no scent");
                check(rain < up * 0.8f, "heavy rain washes the scent out");
            }
        }
        // the lean: downwind, harder in a gale, none in a calm, and proportional to the height of the thing
        {
            const Spr& tall = g.A.oak[0][0];
            const Spr& low = g.A.treeGrass[0][0];
            g.windGust = 1.f;
            g.windDir = V2(1.f, 0.f);
            g.wind = 0.f;
            const float calm = g.windLean(tall);
            g.wind = 1.5f;
            const float east = g.windLean(tall), eastLow = g.windLean(low);
            g.windDir = V2(-1.f, 0.f);
            const float west = g.windLean(tall);
            check(calm == 0.f && east > 2.f && west < -2.f && fabsf(east + west) < 0.01f, "trees and grass lean downwind, harder in a gale, not at all in a calm");
            check(eastLow > 0.f && eastLow < east, "a low plant leans less than a tall tree");
            g.windGust = 0.5f;
        }
        g.animals.clear();
        g.timeAcc = time0;
    }
    // wolves hunt what lives in the snow
    check(((ANIMAL[A_WOLF].preyMask >> A_REINDEER) & 1) && ((ANIMAL[A_WOLF].preyMask >> A_SNOWHARE) & 1), "wolves hunt reindeer and snow hares");
    check((ANIMAL[A_POLARBEAR].preyMask >> A_SEAL) & 1, "polar bears hunt seals");
    // rarity: every species has a named legendary, and the bad-luck protection raises the legendary odds and resets after one
    bool named = true;
    for (int k = 0; k < A_COUNT; k++) named = named && ANIMAL[k].legendName && ANIMAL[k].legendName[0];
    check(named, "every species has a named legendary");
    g.animals.clear();
    int plain = 0, boosted = 0;
    const float pity0 = g.legPity, cool0 = g.legCool;
    for (int i = 0; i < 4000; i++) {
        g.legCool = 0.f;
        g.legPity = 0.f;
        plain += g.rollTier() == TR_LEGENDARY;
        g.legCool = 0.f;
        g.legPity = LEGEND_PITY_MAX;
        boosted += g.rollTier() == TR_LEGENDARY;
    }
    check(boosted > 60 && boosted > plain * 3, "bad-luck protection raises the legendary odds");
    g.legCool = 0.f;
    g.legPity = 0.f;
    int r = TR_LEGENDARY;
    for (int i = 0; i < 50 && r == TR_LEGENDARY; i++) r = g.rollTier(), g.legCool = 0.f;
    check(r != TR_LEGENDARY && g.legPity > 0.f && g.legPity <= LEGEND_PITY_MAX, "a roll that is not a legendary builds up the protection");
    g.legPity = LEGEND_PITY_MAX;
    bool reset = false;
    for (int i = 0; i < 2000 && !reset; i++) {
        g.legCool = 0.f;
        g.legPity = LEGEND_PITY_MAX;
        if (g.rollTier() == TR_LEGENDARY) reset = g.legPity == 0.f;
    }
    check(reset, "a legendary resets the protection");
    // looks follow the moment: ghosts come at night, frost in a blizzard, gold at dawn
    const float day0 = g.dayT;
    g.interior = -1;
    g.delve = -1;
    auto legends = [&](int kind) {
        int n = 0;
        for (int i = 0; i < 30000; i++) {
            g.legCool = 0.f;
            g.legPity = 0.f;
            n += g.rollTier(kind) == TR_LEGENDARY;
        }
        return n;
    };
    g.wxKind = WX_CLEAR;
    g.stormK = 0.f;
    g.dayT = DAY_LEN * 0.2f;
    const int ghostNoon = legends(A_WOLF);  // the wolf's legendary look is ghost
    g.dayT = DAY_LEN * 0.7f;
    const int ghostNight = legends(A_WOLF);
    check(ghostNight > ghostNoon * 3 && ghostNight > 40, "ghost legendaries come mostly at night");
    g.dayT = DAY_LEN * 0.2f;
    const int frostClear = legends(A_REINDEER);  // the reindeer's legendary look is frost
    g.wxKind = WX_BLIZZARD;
    g.stormK = 1.f;
    const int frostBlizzard = legends(A_REINDEER);
    check(frostBlizzard > frostClear * 2 && frostBlizzard > 40, "frost legendaries come in a blizzard");
    g.dayT = DAY_LEN * 0.96f;  // dawn
    g.wxKind = WX_CLEAR;
    g.stormK = 0.f;
    const int goldDawn = legends(A_DEER);  // the deer's legendary look is gold
    g.dayT = DAY_LEN * 0.2f;
    const int goldNoon = legends(A_DEER);
    check(goldDawn > goldNoon * 1.8f && goldDawn > 40, "gold legendaries come at dawn");
    check(g.lookLuck(LK_NONE) == 1.f, "a plain look is not affected by the moment");
    // the bestiary's "when it turns up" hint exists for exactly the looks the moment changes
    bool hintsMatch = true;
    for (int look = 0; look < LK_COUNT; look++) {
        bool varies = false;
        for (int w = 0; w < WX_COUNT && !varies; w++)
            for (float d : {0.0f, 0.2f, 0.45f, 0.7f, 0.96f}) {
                g.wxKind = w;
                g.stormK = 1.f;
                g.dayT = DAY_LEN * d;
                if (fabsf(g.lookLuck(look) - 1.f) > 0.01f) varies = true;
            }
        hintsMatch = hintsMatch && (varies == (lookHint(look) != nullptr));
    }
    check(hintsMatch, "the bestiary hints cover exactly the looks that depend on the moment");
    g.dayT = day0;
    g.legPity = pity0;
    g.legCool = cool0;
    // visibility polygons: a wall or a trunk stops the light at its edge and leaves what is behind it dark; open ground is lit out to the radius
    {
        std::vector<VisSeg> wall = {{20.f, -12.f, 20.f, 12.f}};
        VisEntry v;
        visCast(wall, 0.f, 0.f, 60.f, v);
        check(fabsf(v.reach(40.f, 0.f) - 20.f) < 1.f && v.reach(-40.f, 0.f) >= 59.9f && v.reach(0.f, 40.f) >= 59.9f, "a wall stops the light at its edge, and only in its own direction");
        // a tree in the real world
        const Prop* tree = nullptr;
        for (const Prop& pr : g.world.props) {
            const shadowdef::PropShadow* d = shadowdef::propShadow(pr.kind);
            if (d && (d->shape == shadowdef::SH_TRUNK || d->shape == shadowdef::SH_OAK) && pr.x > 80.f && pr.y > 80.f) {
                tree = &pr;
                break;
            }
        }
        check(tree != nullptr, "there is a tree to test with");
        if (tree) {
            const float lx = tree->x - 30.f, ly = tree->y - 2.f;
            std::vector<VisSeg> segs;
            visGather(g.world, [](size_t) { return true; }, lx, ly, 70.f, segs);
            VisEntry t;
            visCast(segs, lx, ly, 70.f, t);
            const float toward = t.reach(tree->x, tree->y - 2.f), away = t.reach(lx - 40.f, ly);
            check(toward > 18.f && toward < 29.5f, "a tree trunk stops a torch's light at its foot");
            check(away >= 69.f || away > toward + 20.f, "and the light is free on the other side");
            VisEntry gone;
            visGather(g.world, [](size_t) { return false; }, lx, ly, 70.f, segs);  // a felled tree casts nothing
            visCast(segs, lx, ly, 70.f, gone);
            check(gone.reach(tree->x, tree->y - 2.f) > toward + 5.f, "a felled tree no longer blocks the light");
        }
        // a building or a boulder: its tile outline
        int ox = -1, oy = -1;
        for (int ty = 8; ty < MH - 8 && ox < 0; ty++)
            for (int tx = 8; tx < MW - 8; tx++)
                if (g.world.occ[ty][tx] && !g.world.occ[ty][tx - 1] && !g.world.occ[ty][tx - 2] && !g.world.occ[ty][tx - 3] && !g.world.occ[ty][tx - 4]) {
                    ox = tx, oy = ty;
                    break;
                }
        if (ox >= 0) {
            const float lx = ox * (float)TS - 28.f, ly = oy * (float)TS + 8.f;
            std::vector<VisSeg> segs;
            visGather(g.world, [](size_t) { return true; }, lx, ly, 70.f, segs);
            VisEntry t;
            visCast(segs, lx, ly, 70.f, t);
            check(fabsf(t.reach(ox * (float)TS + 20.f, ly) - 28.f) < 2.5f, "a rock or a wall stops the light at the edge of its tiles");
        }
    }
    // put things back
    g.dayT = day0;
    g.animals.clear();
    g.wxKind = wx0;
    g.stormK = storm0;
    g.interior = interior0;
    g.delve = delve0;
    g.loadMapWorld(map0);
    g.p.pos = pos0;
}

// ---- bushes (bush_react.hpp): the table, what a brush sets off, the snow a shrub drops and gets back, the hares it flushes, the cover it gives
void testBushes(SelfTest& t) {
    Game& g = t.g;
    auto check = [&](bool ok, const char* what) { t.check(ok, what); };
    printf("bushes\n");
    const int map0 = g.world.mapId;
    const V2 pos0 = g.p.pos;
    const int interior0 = g.interior, delve0 = g.delve;
    const float time0 = g.timeAcc;
    check(bushDefFor(D_BUSH0, B_HOME).bush && !bushDefFor(D_BUSH0, B_HOME).sheds && bushDefFor(D_BUSH0, B_SNOW).sheds && !bushDefFor(D_TUFT, B_HOME).bush,
          "the bush table: round bushes and snow shrubs are bushes (only the snow ones shed), a tuft of grass is not");
    check(bushDefFor(D_BUSH0, B_HOME).cover > bushDefFor(D_BUSH0, B_SNOW).cover, "a leafy bush hides more than a bare twig shrub");
    g.loadMapWorld(B_SNOW);
    g.animals.clear();
    g.interior = -1;
    g.delve = -1;
    g.p.dead = false;
    g.state = ST_PLAY;
    size_t bi = g.world.decor.size();
    for (size_t i = 0; i < g.world.decor.size() && bi == g.world.decor.size(); i++) {
        const Decor& d = g.world.decor[i];
        if (d.kind == D_BUSH0 && d.x > 200 && d.x < MW * TS - 200 && d.y > 200 && d.y < MH * TS - 200) bi = i;
    }
    check(bi < g.world.decor.size(), "the snowfield has shrubs");
    if (bi == g.world.decor.size()) {
        g.loadMapWorld(map0);
        return;
    }
    g.updateGrass(1.f / 60.f);  // (sets up the per-bush records for this map)
    const Decor b = g.world.decor[bi];
    const V2 bp(b.x, b.y);
    g.p.pos = bp + V2(-20.f, 0.f);
    g.timeAcc = 1000.f;
    check(g.bushSnowK(bi) == 1.f, "a shrub starts out banked with snow");
    const float cover0 = g.bushCoverAt(bp);
    check(cover0 > 0.2f && g.bushCoverAt(bp + V2(40.f, 0.f)) == 0.f || g.bushCoverAt(bp + V2(40.f, 0.f)) < cover0, "standing in a bush gives cover, a few steps away does not");
    g.bushBrushed(bi, bp + V2(-6.f, 0.f), 20.f, true);
    check(g.bushSnowK(bi) == 1.f, "a slow brush leaves the snow on");
    g.timeAcc += 1.f;
    g.bushBrushed(bi, bp + V2(-6.f, 0.f), 60.f, true);
    check(g.bushSnowK(bi) == 0.f && g.bushCoverAt(bp) < cover0, "pushed through hard, a shrub drops its snow and hides less");
    g.timeAcc += g.BUSH_REFILL * 0.5f;
    check(g.bushSnowK(bi) == 0.f, "it stays bare for a good while");
    g.timeAcc += g.BUSH_REFILL * 0.6f;
    check(g.bushSnowK(bi) == 1.f && std::fabs(g.bushCoverAt(bp) - cover0) < 0.001f, "and the snow banks up again");
    // blizzards bring the snow back faster
    g.timeAcc += 1.f;
    g.bushBrushed(bi, bp + V2(-6.f, 0.f), 60.f, true);
    const int wx0 = g.wxKind;
    const float storm0 = g.stormK;
    g.wxKind = WX_BLIZZARD;
    g.stormK = 1.f;
    for (int i = 0; i < 60 * 40; i++) g.updateGrass(1.f / 60.f);  // 40 s of blizzard
    check(g.bushSnowK(bi) > 0.f, "a blizzard banks the snow up faster");
    g.wxKind = wx0;
    g.stormK = storm0;
    // a hare lying up beside the bush bolts when the hunter pushes through; one further off does not
    g.animals.clear();
    g.timeAcc += 10.f;
    Animal* near = g.spawnAnimal(A_SNOWHARE, bp + V2(8.f, 6.f), 0, 0);
    Animal* far = g.spawnAnimal(A_SNOWHARE, bp + V2(90.f, 6.f), 0, 0);
    if (near && far) {
        near->st = AS_WANDER;
        far->st = AS_WANDER;
        g.bushBrushed(bi, bp + V2(-6.f, 0.f), 40.f, true);
        check(near->st == AS_FLEE && far->st == AS_WANDER, "a hare in the bush bolts, one further off does not");
    } else
        check(false, "hares can be spawned beside a shrub");
    // cover against the eyes: an animal 120 px off sees a hunter standing still in a bush less well than one in the open
    g.animals.clear();
    Animal* deer = g.spawnAnimal(A_DEER, bp + V2(120.f, 0.f), 0, 0);
    if (deer) {
        g.p.sprinting = false;
        g.p.walking = false;
        const float inBush = g.sightFactor(*deer, bp, true), inOpen = g.sightFactor(*deer, bp + V2(-40.f, 0.f), true);
        check(inBush < inOpen, "an animal sees a hunter in a bush less well than one standing in the open");
    }
    g.animals.clear();
    g.timeAcc = time0;
    g.interior = interior0;
    g.delve = delve0;
    g.loadMapWorld(map0);
    g.p.pos = pos0;
}

// ---- ice spikes (art/ice_spike_sprites.hpp): they stand in the snowfield, never on a trail, a mined one leaves rubble and comes back small and grows size by size
void testIceSpikes(SelfTest& t) {
    Game& g = t.g;
    auto check = [&](bool ok, const char* what) { t.check(ok, what); };
    printf("ice spikes\n");
    const int map0 = g.world.mapId;
    const V2 pos0 = g.p.pos;
    const int interior0 = g.interior, delve0 = g.delve;
    g.loadMapWorld(B_SNOW);
    g.animals.clear();
    g.interior = -1;
    g.delve = -1;
    g.p.dead = false;
    g.state = ST_PLAY;
    int n = 0, bySize[ICE_SPIKE_STAGES] = {0}, onTrail = 0, unsolid = 0, big = -1;
    for (size_t i = 0; i < g.world.props.size(); i++) {
        const Prop& pr = g.world.props[i];
        if (pr.kind != P_ICESPIKE) continue;
        n++;
        bySize[(int)pr.ph % ICE_SPIKE_STAGES]++;
        const int tx = (int)(pr.x / TS), ty = (int)(pr.y / TS);
        if (g.world.tile[ty][tx] == T_DIRT || g.world.tile[ty][tx] == T_ICE || g.world.tile[ty][tx] == T_WATER) onTrail++;
        if (!g.world.solid[ty][tx]) unsolid++;
        if ((int)pr.ph % ICE_SPIKE_STAGES >= 2 && big < 0 && g.propRsId[i] == RS_ICESPIKE) big = (int)i;
    }
    check(n >= 20, "the snowfield has ice spikes");
    check(bySize[0] + bySize[1] > 0 && bySize[2] + bySize[3] + bySize[4] > 0, "in small and in big sizes");
    check(onTrail == 0, "none stands on a trail or on the water");
    check(unsolid == 0, "each blocks its own tile");
    check(big >= 0, "the spikes are minable (they have a resource)");
    if (big >= 0) {
        const int full = (int)g.world.props[big].ph % ICE_SPIKE_STAGES;
        check(g.spikeStage(big) == full, "a spike shows the size it was made with");
        HState& s = g.propSt[big];
        s.gone = true;
        s.t = 0.5f;
        g.world.setPropSolid(g.world.props[big], false);  // (what finishing it does)
        g.hActive.push_back(big);  // (a prop's id in the active list is its index; a node's has a flag bit on top)
        for (int i = 0; i < 60 * 2; i++) g.updateHarvest(1.f / 60.f);  // the wait is over: it sprouts
        check(s.gone == false && s.fade > 0.f, "a mined spike begins to come back after its wait");
        for (int i = 0; i < 60 * 6; i++) g.updateHarvest(1.f / 60.f);
        check(!s.gone && s.fade <= 0.f && s.stage == 0 && g.spikeStage(big) == 0, "it comes back at the smallest size");
        g.updateHarvest(SPIKE_GROW_SECS + 1.f);
        check(g.spikeStage(big) == 1 || full == 1, "and grows a size every so often");
        for (int k = 0; k < 6; k++) g.updateHarvest(SPIKE_GROW_SECS + 1.f);
        check(g.spikeStage(big) == full && s.stage == 255, "until it is the size it was");
        check(std::find(g.hActive.begin(), g.hActive.end(), big) == g.hActive.end(), "and then it is left alone");
    }
    g.animals.clear();
    g.interior = interior0;
    g.delve = delve0;
    g.loadMapWorld(map0);
    g.p.pos = pos0;
}
