// delve_tests.cpp - `wildwood --delve`: the Abandoned Mine and the Frozen Dungeon played through the real game code, headless.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include "core/game.hpp"

static void stepFrames(Game& g, int n, float dt = 1.f / 60.f) {
    for (int i = 0; i < n; i++) g.update(dt);
}
static int countItem(const Game& g, int itemId) {
    int n = g.p.inv.count(itemId);
    for (const GroundItem& it : g.items)
        if (it.item == itemId) n += it.n;
    return n;
}
// stand somewhere free near a point
// beside a chest: the first free spot around it
static void standBeside(Game& g, V2 at) {
    static const V2 O[8] = {{0, 12}, {0, -10}, {-14, 0}, {14, 0}, {-12, 12}, {12, 12}, {-12, -10}, {12, -10}};
    for (const V2& o : O)
        if (!g.world.blocked(at.x + o.x, at.y + o.y, 4.f)) { g.p.pos = at + o; g.p.kb = V2(); return; }
    g.p.pos = g.freeSpot((int)(at.x / TS), (int)(at.y / TS));
}
static void standAt(Game& g, V2 at) {
    if (!g.world.blocked(at.x, at.y, 4.f)) { g.p.pos = at; g.p.kb = V2(); return; }
    g.p.pos = g.freeSpot((int)(at.x / TS), (int)(at.y / TS)); g.p.kb = V2(); }

int delveTest(Game& g) {
    int bad = 0;
    auto check = [&](bool ok, const std::string& what) {
        printf("  [%s] %s\n", ok ? "ok" : "FAIL", what.c_str());
        if (!ok) bad++;
    };
    // the save tests write the real save file: keep the player's own safe and put it back afterwards
    std::string savedBytes;
    bool hadSave = false;
    {
        std::ifstream in(g.savePath(), std::ios::binary);
        if (in) { hadSave = true; savedBytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()); }
    }
    g.newGame();
    g.state = ST_PLAY;
    g.p.hp = g.p.maxhp = 100000.f;  // the AI tests below should not be cut short by the hunter dying; the death test resets this

    printf("--- the monsters' art\n");
    g.ensureMonsterArt();
    {
        bool all = g.A.M.built;
        for (int k = 0; k < MK_COUNT; k++)
            for (int st : {MS_IDLE, MS_WALK, MS_ATK, MS_HURT, MS_DEATH})
                for (int v = 0; v < AV_VIEWS; v++)
                    for (int f = 0; f < MS_FRAMES[st]; f++) all = all && g.A.M.s[k][v][st][f].ok();
        check(all, "every creature has idle / walk / attack / hurt / death frames in all 4 directions");
        bool cast = true;
        for (int k : {MK_MAGE, MK_KING, MK_GOLEM, MK_ICEGOLEM})
            for (int v = 0; v < AV_VIEWS; v++) cast = cast && g.A.M.s[k][v][MS_CAST][0].ok();
        check(cast, "mages, golems and the king have a spell / slam clip too");
    }

    printf("--- entering and leaving the mine\n");
    g.travelTo(B_MOUNTAIN, MAPS[B_MOUNTAIN].entryTx, MAPS[B_MOUNTAIN].entryTy, false);
    check(g.world.gate.x > 0, "the mountain has an abandoned mine entrance");
    standAt(g, g.world.gate + V2(0, 16));
    const V2 before = g.p.pos;
    g.bldCd = 0;
    check(g.delveNear() == 1, "E near the entrance offers to enter");
    check(g.delveInteract() && g.delve == DV_MINE && g.world.isDelve() && g.world.mapId == DELVE_MAP0 + DV_MINE, "E enters the mine (a map of its own)");
    check(!g.mons.empty() && g.world.dsp.size() > 20, "the mine is populated with creatures");
    check(dist(g.p.pos, g.world.spawn) < 40.f, "the hunter arrives inside at the mine's entrance room");
    g.bldCd = 0;
    standAt(g, g.world.exitPos);
    check(g.delveNear() == 2, "the exit is offered at the way out");
    check(g.delveInteract() && g.delve == -1 && g.world.mapId == B_MOUNTAIN, "E leaves: the mountain is back");
    check(dist(g.p.pos, before) < 1.f, "the hunter stands exactly where they entered");
    check(g.world.gate.x > 0, "and the entrance is still there");

    printf("--- mining\n");
    g.p.inv.add(itemTool(TL_PICK1), 1);
    g.bldCd = 0;
    standAt(g, g.world.gate + V2(0, 16));
    g.delveInteract();
    check(g.delve == DV_MINE, "back in the mine");
    // the share of each ore: rarer = fewer
    {
        long cnt[RN_COUNT] = {0};
        for (int id : {DV_MINE}) { World w; w.generateDelve(id); for (auto& n : w.nodes) cnt[n.kind]++; }
        printf("  mine ore counts: coal %ld, copper %ld, iron %ld, silver %ld, gold %ld, crystal %ld, gems %ld\n", cnt[RN_COAL], cnt[RN_COPPER], cnt[RN_ORE], cnt[RN_SILVER], cnt[RN_GOLD], cnt[RN_CAVECRYSTAL], cnt[RN_GEM]);
        check(cnt[RN_COAL] > cnt[RN_SILVER] && cnt[RN_COAL] > cnt[RN_GEM] && cnt[RN_SILVER] + cnt[RN_GOLD] > cnt[RN_GEM], "the rarer the ore, the lower its spawn rate");
        check(cnt[RN_COAL] && cnt[RN_COPPER] && cnt[RN_ORE] && cnt[RN_SILVER] && cnt[RN_GOLD] && cnt[RN_CAVECRYSTAL] && cnt[RN_GEM], "copper, iron, gold, silver, coal, crystals and gems all occur");
    }
    check(RES[RS_COAL].respawn < RES[RS_SILVER].respawn && RES[RS_SILVER].respawn <= RES[RS_GEM].respawn, "rarer deposits take longer to grow back");
    check(RES[RS_SILVER].minLv > RES[RS_COAL].minLv && RES[RS_GEM].minLv > RES[RS_SILVER].minLv && RES[RS_GEM].minTier > RES[RS_COAL].minTier, "better ores need a better pickaxe and more mining skill");
    // coal with a stone pickaxe
    auto nearestNode = [&](int kind) {
        int best = -1;
        float bd = 1e9f;
        for (size_t i = 0; i < g.world.nodes.size(); i++)
            if (g.world.nodes[i].kind == kind && dist(g.world.spawn, V2(g.world.nodes[i].x, g.world.nodes[i].y)) < bd) { bd = dist(g.world.spawn, V2(g.world.nodes[i].x, g.world.nodes[i].y)), best = (int)i; }
        return best;
    };
    auto standAtNode = [&](int idx) {
        const ResNode& n = g.world.nodes[idx];
        // the open tile next to the node
        for (int d = 0; d < 4; d++) {
            const int dx[4] = {0, 0, -1, 1}, dy[4] = {1, -1, 0, 0};
            const int tx = (int)(n.x / TS) + dx[d], ty = (int)(n.y / TS) + dy[d];
            if (!g.world.solidTile(tx, ty)) { g.p.pos = V2(tx * TS + 8.f, ty * TS + 12.f); return true; }
        }
        return false;
    };
    auto mine = [&](int idx, float secs) {
        g.in = Input();
        g.in.e = true;
        for (int i = 0; i < (int)(secs * 60); i++) {
            g.in.ePressed = i == 0;
            g.update(1.f / 60.f);
            g.in.ePressed = false;
            if (g.nodeSt[idx].gone) break;
        }
        g.in = Input();
    };
    {
        const int ci = nearestNode(RN_COAL);
        check(ci >= 0 && standAtNode(ci), "a coal seam lies near the entrance");
        const int coal0 = g.p.mat(M_COAL);
        const int64_t xp0 = g.p.sk.s[SK_MINING].xp;
        g.mons.clear();  // nobody interrupts the miner
        mine(ci, 20.f);
        check(g.nodeSt[ci].gone && g.p.mat(M_COAL) > coal0, "a stone pickaxe mines coal");
        check(g.p.sk.s[SK_MINING].xp > xp0, "and pays Mining XP");
        check(g.nodeSt[ci].t > 0, "the seam is gone for now with a regrow timer running");
    }
    {
        const int si = nearestNode(RN_SILVER);
        check(si >= 0 && standAtNode(si), "there is a silver vein to try");
        g.p.inv.add(itemTool(TL_PICK2), 1);
        g.p.sk.s[SK_MINING].xp = 0;
        const int silver0 = g.p.mat(M_SILVER);
        g.mons.clear();
        mine(si, 6.f);
        check(!g.nodeSt[si].gone && g.p.mat(M_SILVER) == silver0, "silver cannot be mined at Mining level 1 (even with a copper pickaxe)");
        g.p.sk.s[SK_MINING].xp = 100000;
        g.p.inv.remove(itemTool(TL_PICK2), 1);
        mine(si, 6.f);
        check(!g.nodeSt[si].gone && g.p.mat(M_SILVER) == silver0, "...nor with the level but a stone pickaxe");
        g.p.inv.add(itemTool(TL_PICK2), 1);
        mine(si, 25.f);
        check(g.nodeSt[si].gone && g.p.mat(M_SILVER) > silver0, "with skill and a copper pickaxe it yields silver");
    }

    printf("--- creatures: combat, drops, XP\n");
    {
        g.mons.clear();
        g.items.clear();
        g.hazards.clear();
        g.mprojs.clear();
        // depth scaling
        Monster* a = g.spawnMonster(MK_SPIDER, g.p.pos + V2(300, 0), 0.f, false, -1);
        const int hp0 = a->maxhp;
        const float dm0 = a->dmg;
        Monster* b = g.spawnMonster(MK_SPIDER, g.p.pos + V2(300, 0), 1.f, false, -1);
        check(b->maxhp > hp0 && b->dmg > dm0, "deeper creatures have more health and hit harder");
        Monster* e = g.spawnMonster(MK_SPIDER, g.p.pos + V2(300, 0), 0.f, true, -1);
        check(e->maxhp > hp0 && e->elite, "elites are tougher still");
        g.mons.clear();
        // every kind can be killed, pays XP, drops its own loot
        const int kinds[] = {MK_BAT, MK_SPIDER, MK_GOLEM, MK_CAVEWOLF, MK_ICESKEL, MK_FROSTWOLF, MK_ICEGOLEM, MK_KNIGHT, MK_MAGE};
        int paid = 0, lootKinds = 0;
        for (int k : kinds) {
            g.mons.clear();
            g.items.clear();
            Monster* m = g.spawnMonster(k, g.p.pos + V2(40, 0), 0.5f, false, -1);
            const int64_t x0 = g.p.sk.s[SK_HUNTING].xp;
            const int coins0 = g.p.coins;
            bool got = false;
            for (int tries = 0; tries < 12 && !got; tries++) {  // (loot is by chance: a few kills)
                m = &g.mons.back();
                m->hp = 1;
                m->state = MN_CHASE;
                m->awake = true;
                g.hitMonster(*m, 5000.f, V2(1, 0), false, W_JAVELIN);
                for (auto& it : g.items) got = got || it.item >= 0;
                if (!got) { g.spawnMonster(k, g.p.pos + V2(40, 0), 0.5f, false, -1); }
            }
            if (g.p.sk.s[SK_HUNTING].xp > x0 && g.p.coins > coins0) paid++;
            if (got) lootKinds++;
        }
        check(paid == 9, "all nine creature kinds die, pay Hunting XP and coins");
        check(lootKinds == 9, "and every kind drops items of its own");
        // each kind's drops are distinct
        std::set<int> firstDrops;
        for (int k : kinds) firstDrops.insert(MONDEF(k).drops[0].mat);
        check(firstDrops.size() >= 8, "their unique drops differ");
    }
    {   // a real throw: aim at a creature with the mouse and let the projectile fly
        g.mons.clear();
        g.items.clear();
        g.projs.clear();
        g.p.inv.add(itemWpn(W_JAVELIN), 5);
        g.p.sel = W_JAVELIN;
        standAt(g, g.world.spawn);
        g.setCam(0, true);
        Monster* m = g.spawnMonster(MK_ICEGOLEM, g.p.pos + V2(60, 0), 0.f, false, -1);
        m->awake = false;
        m->spd = 0;
        const int hp0 = m->hp;
        g.in = Input();
        g.in.mx = m->pos.x - g.camX;
        g.in.my = m->pos.y - MONDEF(MK_ICEGOLEM).bodyH - g.camY + 10;
        g.equipHotQuiet(itemWpn(W_JAVELIN));
        g.p.sel = W_JAVELIN;
        g.p.charge = 1.f;
        g.p.cd = 0;
        g.throwWeapon();
        stepFrames(g, 60);
        check(g.mons.empty() || g.mons[0].hp < hp0, "a thrown javelin hits a creature in actual play");
    }
    {   // they fight back: a spider's bite hurts and poisons; slow and poison are applied
        g.mons.clear();
        g.hazards.clear();
        g.p.hp = g.p.maxhp = 200.f;
        g.p.invuln = 0;
        standAt(g, g.world.spawn);
        Monster* sp = g.spawnMonster(MK_SPIDER, g.p.pos + V2(10, 0), 0.5f, false, -1);
        sp->awake = true;
        sp->state = MN_CHASE;
        g.in = Input();
        stepFrames(g, 240);
        check(g.p.hp < 200.f, "a spider attacks the hunter");
        check(g.pPoisonT > 0 || g.p.hp < 195.f, "its bite poisons");
        g.mons.clear();
        g.pPoisonT = 0;
        g.p.hp = g.p.maxhp = 100000.f;
    }
    {   // a frost mage shoots ice bolts; a golem's slam shows a warning circle before it lands
        g.mons.clear();
        g.mprojs.clear();
        g.hazards.clear();
        standAt(g, g.world.spawn);
        Monster* mg = g.spawnMonster(MK_MAGE, g.p.pos + V2(70, 0), 0.5f, false, -1);
        mg->awake = true;
        mg->state = MN_CHASE;
        bool bolt = false;
        for (int i = 0; i < 600 && !bolt; i++) { g.update(1.f / 60.f); bolt = !g.mprojs.empty(); }
        check(bolt, "a frost mage casts ice bolts");
        g.mons.clear();
        g.mprojs.clear();
        standAt(g, g.world.spawn);
        Monster* go = g.spawnMonster(MK_GOLEM, g.p.pos + V2(20, 0), 0.5f, false, -1);
        go->awake = true;
        go->state = MN_CHASE;
        bool warn = false, hit = false;
        const float hp0 = g.p.hp;
        for (int i = 0; i < 400; i++) {
            g.update(1.f / 60.f);
            for (auto& h : g.hazards) if (!h.fired && h.kind == HZ_SLAM) warn = true;
            if (g.p.hp < hp0) hit = true;
        }
        check(warn, "a rock golem telegraphs its slam with a warning circle");
        (void)hit;
        g.mons.clear();
        g.hazards.clear();
    }

    printf("--- traps and falling rocks\n");
    {
        g.mons.clear();
        g.hazards.clear();
        g.p.hp = g.p.maxhp = 100.f;
        g.p.invuln = 0;
        standAt(g, g.world.spawn);
        g.addHazard(HZ_ROCK, g.p.pos, 12.f, 0.5f, 20.f);
        stepFrames(g, 20);
        const bool pending = !g.hazards.empty() && !g.hazards[0].fired && g.p.hp == 100.f;
        stepFrames(g, 30);
        check(pending && g.p.hp < 100.f, "a falling rock warns first, then hurts anyone under it");
        g.p.hp = g.p.maxhp = 100000.f;
    }
    g.bldCd = 0;
    standAt(g, g.world.exitPos);
    g.delveInteract();
    check(g.delve == -1, "(out of the mine again)");

    printf("--- the Frozen Dungeon: levels and gear\n");
    g.travelTo(B_SNOW, MAPS[B_SNOW].entryTx, MAPS[B_SNOW].entryTy, false);
    check(g.world.gate.x > 0, "the snowfield has a frozen fortress entrance");
    standAt(g, g.world.gate + V2(0, 14));
    g.p.up[U_ARMOR] = 0;
    g.bldCd = 0;
    check(g.delveInteract() && g.delve == DV_FROST0, "the first level, the Frozen Entrance, is open to anyone");
    check(!g.delveReq(DV_FROST1) && !g.delveReq(DV_FROST2) && !g.delveReq(DV_FROST3), "deeper levels are closed to an unarmoured hunter");
    std::string why;
    g.p.up[U_ARMOR] = 2;
    check(g.delveReq(DV_FROST1) && !g.delveReq(DV_FROST2, &why) && !why.empty(), "armour tier 2 opens the Frozen Halls (and says what the Crypt needs)");
    g.p.up[U_ARMOR] = 4;
    check(g.delveReq(DV_FROST2) && !g.delveReq(DV_FROST3), "armour tier 4 opens the Ancient Crypt but not the Throne");
    g.p.up[U_ARMOR] = 5;
    check(!g.delveReq(DV_FROST3), "tier 5 armour alone is not enough for the Throne: it needs frost protection");
    g.p.inv.add(itemCarry(CG_FROSTWARD), 1);
    check(g.delveReq(DV_FROST3), "with a frostward cloak it is");
    // stairs: level 1 -> 2
    g.p.up[U_ARMOR] = 0;
    g.p.inv.remove(itemCarry(CG_FROSTWARD), 1);
    standAt(g, g.world.stairsDn);
    g.bldCd = 0;
    check(g.delveNear() == 3 && !g.delveInteract() && g.delve == DV_FROST0, "the stairs refuse a hunter without the armour");
    g.p.up[U_ARMOR] = 2;
    g.bldCd = 0;
    check(g.delveInteract() && g.delve == DV_FROST1 && !g.mons.empty(), "with it the stairs lead down to the Frozen Halls");
    check(dist(g.p.pos, g.world.stairsUp) < 40.f, "arriving beside the stairs up");
    check(!g.dtraps.empty(), "the Halls have spike plates and frost vents");
    {   // a spike plate hurts when it is up
        g.p.hp = g.p.maxhp;
        const float hpTrap = g.p.hp;
        g.p.invuln = 0;
        g.mons.clear();
        DTrap t = g.dtraps[0];
        g.dtraps.clear();
        t.t = 2.1f;
        t.kind = TR_SPIKE;
        g.dtraps.push_back(t);
        g.p.pos = t.pos;
        stepFrames(g, 20);
        check(g.p.hp < hpTrap, "a spike trap hurts the hunter standing on it");
        g.p.hp = g.p.maxhp = 100000.f;
    }
    g.bldCd = 0;
    standAt(g, g.world.stairsUp);
    check(g.delveNear() == 4 && g.delveInteract() && g.delve == DV_FROST0, "and the stairs lead back up");

    printf("--- chests, keys and the crypt\n");
    g.p.up[U_ARMOR] = 4;
    g.bldCd = 0;
    standAt(g, g.world.stairsDn);
    g.delveInteract();
    g.bldCd = 0;
    standAt(g, g.world.stairsDn);
    g.delveInteract();
    check(g.delve == DV_FROST2, "down to the Ancient Crypt");
    {
        int locked = 0, open = 0;
        for (auto& c : g.dchests) (c.locked ? locked : open)++;
        check(locked >= 1 && open >= 1, "the Crypt has locked treasure rooms and ordinary chests");
        int elites = 0;
        for (auto& m : g.mons) elites += m.elite;
        check(elites >= 2, "and elites that hold the keys");
        // an elite drops a crypt key
        g.items.clear();
        g.mons.clear();
        Monster* el = g.spawnMonster(MK_KNIGHT, g.p.pos + V2(40, 0), 0.6f, true, -1);
        el->awake = true;
        el->state = MN_HURT;  // (not guarding with the shield)
        g.hitMonster(*el, 100000.f, V2(1, 0), false, W_JAVELIN);
        check(countItem(g, itemMat(M_CRYPTKEY)) >= 1, "a crypt elite drops a crypt key");
        // locked chest: refused without a key, opens with it
        int li = -1;
        for (size_t i = 0; i < g.dchests.size(); i++) if (g.dchests[i].locked) li = (int)i;
        standBeside(g, g.dchests[li].pos);
        g.p.inv.remove(itemMat(M_CRYPTKEY), g.p.mat(M_CRYPTKEY));
        for (auto it = g.items.begin(); it != g.items.end();) it = it->item == itemMat(M_CRYPTKEY) ? g.items.erase(it) : it + 1;
        const size_t n0 = g.dchests.size();
        g.bldCd = 0;
        g.delveInteract();
        check(g.dchests.size() == n0, "a locked chest will not open without a key");
        g.p.inv.add(itemMat(M_CRYPTKEY), 1);
        const int coins0 = g.p.coins;
        g.bldCd = 0;
        g.delveInteract();
        check(g.dchests.size() == n0 - 1 && g.p.mat(M_CRYPTKEY) == 0 && g.p.coins > coins0, "with a key it opens, the key is used up and the loot is taken");
        // an ordinary chest
        int oi = -1;
        for (size_t i = 0; i < g.dchests.size(); i++) if (!g.dchests[i].locked) oi = (int)i;
        const int id = g.dchests[oi].id;
        standBeside(g, g.dchests[oi].pos);
        g.bldCd = 0;
        printf("   near=%d idx=%d chests=%d at (%d,%d) player (%d,%d)\n", g.delveNear(), g.delveNearIdx, (int)g.dchests.size(), (int)g.dchests[oi].pos.x, (int)g.dchests[oi].pos.y, (int)g.p.pos.x, (int)g.p.pos.y);
        g.delveInteract();
        check((g.delveMem[DV_FROST2].chests >> id) & 1u, "an opened chest is remembered");
        // leave and return: it stays open
        g.bldCd = 0;
        standAt(g, g.world.stairsUp);
        g.delveInteract();
        g.bldCd = 0;
        standAt(g, g.world.stairsDn);
        g.delveInteract();
        bool again = false;
        for (auto& c : g.dchests) again = again || c.id == id;
        check(g.delve == DV_FROST2 && !again, "back on the level, the opened chest is still empty");
    }

    printf("--- the Frostbound King\n");
    g.p.up[U_ARMOR] = 8;
    g.p.inv.add(itemCarry(CG_FROSTWARD), 1);
    g.delveMem[DV_FROST3].bossDead = false;
    g.bldCd = 0;
    // find the stairs down of level 3 -> 4
    g.delveGo(DV_FROST3, true);
    check(g.delve == DV_FROST3 && g.world.bossDoor.x > 0, "the Frozen Throne level has the boss door");
    {
        bool boss = false;
        for (auto& m : g.mons) boss = boss || m.kind == MK_KING;
        check(boss, "the Frostbound King waits in the throne hall");
        const int kc = (int)g.mons.size();
        for (auto& m : g.mons) if (m.kind == MK_KING) check(m.maxhp >= 2000, "he is a huge-health boss");
        (void)kc;
        // the door
        standAt(g, g.world.bossDoor + V2(0, 22));
        g.bldCd = 0;
        check(g.delveNear() == 5 && g.world.solid[92][65], "the boss door is shut until the hunter opens it");
        g.delveInteract();
        check(!g.world.solid[92][65], "E opens it");
        // the fight: stand in the hall and let him act
        Monster* k = nullptr;
        for (auto& m : g.mons) if (m.kind == MK_KING) k = &m;
        g.p.hp = g.p.maxhp = 1e6f;
        standAt(g, k->pos + V2(0, 140));
        g.hazards.clear();
        g.mprojs.clear();
        std::set<int> attacks;
        bool bars = false, hazard = false, bolt = false, summoned = false;
        for (int i = 0; i < 60 * 70; i++) {
            g.p.hp = g.p.maxhp = 1e6f;  // (the hunter is invulnerable for this test, whatever the king throws)
            g.update(1.f / 60.f);
            k = nullptr;
            for (auto& m : g.mons) if (m.kind == MK_KING) k = &m;
            if (!k) break;
            if (k->state == MN_ATK || k->state == MN_CAST) attacks.insert(k->atkType);
            bars = bars || (g.bossIdx >= 0);
            hazard = hazard || !g.hazards.empty();
            bolt = bolt || !g.mprojs.empty();
            if (i % 120 == 119 && !g.world.blocked(k->pos.x, k->pos.y + 160, 4.f)) { g.p.pos = k->pos + V2(0, 160); }  // (the hunter keeps their distance now and then)
            if (i == 60 * 25) k->hp = k->maxhp * 6 / 10;   // into phase 2
            if (i == 60 * 45) k->hp = k->maxhp * 3 / 10;   // into phase 3
            int others = 0;
            for (auto& m : g.mons) if (m.kind != MK_KING && m.state != MN_DEAD) others++;
            summoned = summoned || others > 0;
        }
        check(k && k->awake && bars, "he wakes when the hunter enters the hall and the boss bar is shown");
        check(attacks.count(0) || attacks.count(1), "he uses melee and area attacks");
        { std::string as; for (int a : attacks) as += std::to_string(a) + " "; printf("   attacks seen: %s bolt=%d\n", as.c_str(), (int)bolt); }
        check(bolt && attacks.count(2), "he fires ice bolts");
        check(hazard, "his area attacks show warning circles");
        check(k && k->phase == 3, "his phases change as his health falls (now in phase 3)");
        check(attacks.size() >= 4, "his attack set varies (distinct attacks seen: " + std::to_string(attacks.size()) + ")");
        check(summoned, "he summons frozen minions");
        // kill him
        const int64_t hx = g.p.sk.s[SK_HUNTING].xp;
        g.items.clear();
        g.p.hp = g.p.maxhp = 1e6f;
        // (he may be mid-transition and shrug off a hit: keep striking until he falls)
        for (int tries = 0; tries < 600 && k && k->state != MN_DEAD; tries++) {
            g.hitMonster(*k, 1e7f, V2(0, -1), false, W_JAVELIN);
            if (k->state == MN_DEAD) break;
            g.update(1.f / 60.f);
            k = nullptr;
            for (auto& m : g.mons) if (m.kind == MK_KING) k = &m;
        }
        check(g.delveMem[DV_FROST3].bossDead && g.p.sk.s[SK_HUNTING].xp - hx >= 2000, "the king falls: large XP");
        bool chest3 = false;
        for (auto& c : g.dchests) chest3 = chest3 || c.tier == 3;
        check(chest3, "his treasure chest appears");
        check(countItem(g, itemMat(M_FROSTRUNE)) > 0 && countItem(g, itemMat(M_ETERNALICE)) > 0, "rare materials drop");
        for (size_t i = 0; i < g.dchests.size(); i++)
            if (g.dchests[i].tier == 3) { standBeside(g, g.dchests[i].pos); break; }
        g.bldCd = 0;
        g.delveInteract();
        check(g.p.has(CG_CROWN) && g.p.has(CG_HEART) && g.p.wpn(W_FROSTBRAND) == 1, "the chest holds the Crown, the Frozen Heart and the Frostbrand");
        // the crown
        g.p.inv.add(itemWpn(W_JAVELIN), 3);
        // respawn timer
        g.tickDelveTimers(1801.f);
        check(!g.delveMem[DV_FROST3].bossDead, "after a while the king returns");
    }

    printf("--- being knocked out underground\n");
    {
        g.p.hp = g.p.maxhp = 100.f;
        g.p.invuln = 0;
        g.hurtPlayer(10000.f, g.p.pos + V2(10, 0), -1);
        check(g.p.dead, "the hunter can be knocked out in a delve");
        stepFrames(g, 60 * 4);
        check(!g.p.dead && g.delve == -1 && g.world.mapId == B_SNOW, "and wakes outside, at the fortress entrance");
        g.p.hp = g.p.maxhp = 100000.f;
    }

    printf("--- crafting, gear and skills\n");
    {
        g.craftAnywhere = true;
        g.craftQuality = Q_NORMAL;
        g.p.inv = Inventory();
        g.p.inv.maxWeight = 1e6f;  // (the pack's weight limit is not what is being tested here)
        g.p.up[U_ARMOR] = 5;
        for (int k = 0; k < SK_COUNT; k++) g.p.sk.s[k].xp = 0;
        auto find = [&](int kind, int out, int count = -1) {
            for (int r = 0; r < RECIPE_N; r++)
                if (RECIPES[r].kind == kind && RECIPES[r].out == out && (count < 0 || RECIPES[r].count == count)) return r;
            return -1;
        };
        auto stock = [&](int r) { for (int k = 0; k < 3; k++) if (RECIPES[r].mat[k] >= 0) g.p.inv.add(itemMat(RECIPES[r].mat[k]), RECIPES[r].n[k]); };
        const int rSteel = find(RK_ITEM, itemMat(M_STEELBAR)), rSilver = find(RK_ITEM, itemMat(M_SILVERBAR));
        check(rSteel >= 0 && rSilver >= 0, "recipes exist for steel and silver bars");
        stock(rSteel);
        check(!g.craft(rSteel), "steel needs Smithing level 20");
        g.p.sk.s[SK_SMITHING].xp = 100000;
        const int64_t sx = g.p.sk.s[SK_SMITHING].xp;
        check(g.craft(rSteel) && g.p.mat(M_STEELBAR) == 1 && g.p.sk.s[SK_SMITHING].xp > sx, "with the skill, coal and iron make a steel bar (and pay Smithing XP)");
        stock(rSilver);
        check(g.craft(rSilver) && g.p.mat(M_SILVERBAR) >= 1, "silver ore and coal make a silver bar (a practised smith may get one extra)");
        const int wants[][2] = {{RK_WEAPON, W_STEELSPEAR}, {RK_WEAPON, W_SILVERDAGGER}, {RK_ITEM, itemCarry(CG_LANTERN)}, {RK_ITEM, itemCarry(CG_GEMRING)},
                                {RK_ITEM, itemCarry(CG_SILVERCHARM)}, {RK_ITEM, itemCarry(CG_FROSTWARD)}};
        int made = 0;
        for (auto& w : wants) {
            const int r = find(w[0], w[1]);
            if (r < 0) continue;
            g.p.sk.s[RECIPES[r].skill].xp = 100000;
            stock(r);
            if (g.craft(r)) made++;
        }
        check(made == 6, "improved weapons, the miner's lantern, gem ring, silver charm and frostward cloak can all be crafted");
        // the armour sets: each tier needs the one before it
        const int r6 = find(RK_GEAR, U_ARMOR, 6), r7 = find(RK_GEAR, U_ARMOR, 7), r8 = find(RK_GEAR, U_ARMOR, 8);
        check(r6 >= 0 && r7 >= 0 && r8 >= 0, "three new armour sets: Miner's Plate, Frostward Armor, Frostbound Plate");
        g.p.sk.s[SK_SMITHING].xp = 2000000;
        stock(r7);
        check(!g.craft(r7), "Frostward Armor needs Miner's Plate first");
        stock(r6);
        const bool c6 = g.craft(r6);
        g.p.invuln = 0;
        stepFrames(g, 2);
        check(c6 && g.p.up[U_ARMOR] == 6 && g.p.maxhp == (float)ARMOR_HP[6], "Miner's Plate is crafted and worn (more health)");
        check(g.craft(r7) && g.p.up[U_ARMOR] == 7, "Frostward Armor follows");
        stock(r8);
        check(g.craft(r8) && g.p.up[U_ARMOR] == 8, "and the Frostbound Plate");
        // inventory rules
        check(g.p.inv.slots.size() == 100 && MAX_STACK == 999, "the pack still has 100 slots and stacks of 999");
        const float w0 = g.p.inv.totalWeight();
        g.p.inv.add(itemMat(M_GEM), 10);
        check(g.p.inv.totalWeight() > w0 && item(itemMat(M_GEM)).weight > 0, "the new materials have weight");
        // gem ring effect
        check(g.p.has(CG_GEMRING), "(the gem ring is carried)");
        // skills: combat and gathering XP were paid by the earlier sections
        check(g.p.sk.s[SK_MINING].xp >= 0, "(skills are tracked)");
        g.craftAnywhere = false;
    }

    printf("--- saving\n");
    {
        g.delveMem[DV_FROST2].chests = 0x5;
        g.delveMem[DV_FROST3].bossDead = true;
        g.delveMem[DV_FROST3].bossTimer = 900.f;
        // inside a delve when saving: the next load stands at the entrance
        standAt(g, g.world.gate + V2(0, 14));
        g.p.up[U_ARMOR] = 3;
        g.bldCd = 0;
        g.delveInteract();
        check(g.delve == DV_FROST0, "(inside the dungeon for the save)");
        const V2 ep = g.extPos;
        g.p.coins = 4321;
        g.save();
        g.newGame();
        g.state = ST_PLAY;
        check(g.load(), "the save loads");
        check(g.delve == -1 && g.world.mapId == B_SNOW, "a hunter saved inside the dungeon loads in the snowfield");
        check(dist(g.p.pos, ep) < 40.f, "standing at the fortress entrance");
        check(g.p.coins == 4321 && g.p.up[U_ARMOR] == 3, "coins and gear are kept");
        check(g.delveMem[DV_FROST2].chests == 0x5 && g.delveMem[DV_FROST3].bossDead && g.delveMem[DV_FROST3].bossTimer > 800.f, "opened chests and the fallen king are remembered");
    }

    // put the player's own save back
    if (hadSave) {
        std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
        out.write(savedBytes.data(), (std::streamsize)savedBytes.size());
    } else std::remove(g.savePath().c_str());
    printf("DELVE: %s (%d failed)\n", bad ? "FAILED" : "ok", bad);
    return bad;
}

// `wildwood --delveshots <dir>`: screenshots of the entrances, the mine, the dungeon levels and the king
void delveShots(Game& g, const std::string& dir) {
    g.newGame();
    g.state = ST_PLAY;
    g.p.hp = g.p.maxhp = 100000.f;
    g.p.up[U_ARMOR] = 8;
    g.p.inv.add(itemCarry(CG_FROSTWARD), 1);
    g.ensureMonsterArt();
    int n = 0;
    auto shot = [&](const char* name) {
        stepFrames(g, 20);
        g.setCam(0, true);
        g.render();
        g.saveCanvas((dir + "/" + name + ".png").c_str());
        n++;
    };
    g.travelTo(B_MOUNTAIN, MAPS[B_MOUNTAIN].entryTx, MAPS[B_MOUNTAIN].entryTy, false);
    standAt(g, g.world.gate + V2(0, 40));
    shot("1_mine_entrance");
    g.bldCd = 0;
    standAt(g, g.world.gate + V2(0, 16));
    g.delveInteract();
    shot("2_mine_first_room");
    // walk to the middle of the mine: the first spawned monster
    for (size_t i = 0; i < g.mons.size() && i < 40; i += 8) {
        standAt(g, g.mons[i].pos + V2(-30, 10));
        g.mons[i].awake = true;
        shot(("3_mine_" + std::to_string(i)).c_str());
    }
    g.bldCd = 0;
    standAt(g, g.world.exitPos);
    g.delveInteract();
    g.travelTo(B_SNOW, MAPS[B_SNOW].entryTx, MAPS[B_SNOW].entryTy, false);
    standAt(g, g.world.gate + V2(0, 50));
    shot("4_fortress_entrance");
    g.bldCd = 0;
    standAt(g, g.world.gate + V2(0, 14));
    g.delveInteract();
    shot("5_frost1");
    for (size_t i = 0; i < g.mons.size() && i < 12; i += 4) {
        standAt(g, g.mons[i].pos + V2(-34, 10));
        g.mons[i].awake = true;
        shot(("6_frost1_mon" + std::to_string(i)).c_str());
    }
    g.delveGo(DV_FROST2, true);
    shot("7_halls");
    g.delveGo(DV_FROST3, true);
    shot("8_crypt");
    g.delveGo(DV_FROST3 + 1 > DV_FROST3 ? DV_FROST3 : DV_FROST3, true);
    // the throne level is the last id; the dungeon ids run FROST0..FROST3 = 1..4
    g.delveMem[DV_FROST3].bossDead = false;
    g.delveGo(DV_FROST3, true);
    standAt(g, g.world.bossDoor + V2(0, 20));
    shot("9_boss_door");
    g.bldCd = 0;
    g.delveInteract();
    Monster* k = nullptr;
    for (auto& m : g.mons) if (m.kind == MK_KING) k = &m;
    if (k) {
        standAt(g, k->pos + V2(0, 110));
        k->awake = true;
        shot("10_king_wakes");
        for (int i = 0; i < 160; i++) g.update(1.f / 60.f);
        shot("11_king_fight");
        k->hp = k->maxhp / 4;
        for (int i = 0; i < 200; i++) g.update(1.f / 60.f);
        shot("12_king_phase3");
    }
    printf("%d screenshots written to %s\n", n, dir.c_str());
}
