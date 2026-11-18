// selftest_town.cpp - `--selftest`: town NPCs, HUD feedback, wildlife and audio.
#include <map>
#include "tests/selftest_ctx.hpp"

void testTownAndFeedback(SelfTest& t) {
    [[maybe_unused]] Game& g = t.g;
    [[maybe_unused]] auto check = [&](bool ok, const char* what) { t.check(ok, what); };
    [[maybe_unused]] const int MEAT = t.MEAT, HIDE = t.HIDE, BONE = t.BONE;
    [[maybe_unused]] const int R_BONE = t.R_BONE, R_ARROW = t.R_ARROW, R_BOLT = t.R_BOLT, R_BANDAGE = t.R_BANDAGE;
    [[maybe_unused]] auto fillWithJunk = [&]() { t.fillWithJunk(); };
    [[maybe_unused]] auto findRecipe = [&](int kind, int out, int firstMat) { return t.findRecipe(kind, out, firstMat); };
    [[maybe_unused]] auto writeFile = [&](const std::string& text) { t.writeFile(text); };
    [[maybe_unused]] auto removeAll = [&]() { t.removeAll(); };
    printf("town: a clearing - nothing to gather in or by the town or on its roads, no wild animals\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        // the original area has no tree wall around it any more: its outer two tiles are not walled off
        {
            int walled = 0, tiles = 0;
            for (int ty = COY; ty < COY + CH; ty++)
                for (int tx = COX; tx < COX + CW; tx++)
                    if (tx < COX + 2 || ty < COY + 2 || tx >= COX + CW - 2 || ty >= COY + CH - 2) {
                        tiles++;
                        if (g.world.solid[ty][tx] && g.world.tile[ty][tx] != T_WATER) walled++;
                    }
            check(walled < tiles / 4, "no tree wall around the original area");
        }
        // every resource (nodes and the choppable props) is out of the town, its 2-tile margin and the four roads leading out
        auto bad = [&](float px, float py) {
            const int tx = (int)(px / TS) - COX, ty = (int)((py - 1.f) / TS) - COY;
            if (tx >= TOWN_X0 - 2 && tx < TOWN_X1 + 2 && ty >= TOWN_Y0 - 2 && ty < TOWN_Y1 + 2) return true;
            if (ty >= 23 && ty <= 27 && ((tx >= 12 && tx < TOWN_X0) || (tx >= TOWN_X1 && tx < 76))) return true;
            if (tx >= 42 && tx <= 45 && ((ty >= 8 && ty < TOWN_Y0) || (ty >= TOWN_Y1 && ty < 52))) return true;
            return false;
        };
        int nodesIn = 0, propsIn = 0, townTrees = 0;
        for (auto& n : g.world.nodes) nodesIn += bad(n.x, n.y);
        for (size_t i = 0; i < g.world.props.size(); i++) {
            const Prop& pr = g.world.props[i];
            if (g.propRsId.size() > i && g.propRsId[i] >= 0 && bad(pr.x, pr.y)) propsIn++;
            if (g.world.inTown(pr.x, pr.y) && (pr.kind == P_OAK0 || pr.kind == P_OAK1 || pr.kind == P_PINE)) townTrees++;
        }
        check(nodesIn == 0, "no plant, ore or other node is in the town, beside it or on the roads");
        check(propsIn == 0, "no choppable tree or minable rock is there either");
        check(townTrees > 0, "the town still has its own trees...");
        g.initHarvest();
        int chopTown = 0;
        for (size_t i = 0; i < g.world.props.size(); i++)
            if (g.world.inTown(g.world.props[i].x, g.world.props[i].y) && g.propRsId[i] >= 0) chopTown++;
        check(chopTown == 0, "...as scenery that cannot be chopped");
        check(g.world.inTown(g.world.spawn.x, g.world.spawn.y) && !g.world.inTown(g.world.village.x - 2000, g.world.village.y), "inTown knows the town rectangle");
        // wildlife: spread over the whole home forest, never in or by the town, and none walks in
        g.animals.clear();
        for (int i = 0; i < 800; i++) g.trySpawn(i % 4, 0, false);
        int inTown = 0, far = 0, wild = 0;
        for (auto& a : g.animals) {
            inTown += g.world.inTown(a.pos.x, a.pos.y, 48.f);
            far += dist(a.pos, g.world.village) > 1000.f;  // out beyond the original area's own tree wall
            wild += a.biome == B_HOME;
        }
        check(!g.animals.empty() && inTown == 0, "no wild animal ever spawns in or by the town");
        check(far > (int)g.animals.size() / 4 && wild == (int)g.animals.size(), "animals spread across the whole forest, not in a ring around the village");
        // the hours of the day change who is about: rabbits keep to daylight, deer are busiest at dawn and dusk, boars are about all the
        // time, wolves hunt at night (and the surplus slips away when the hour turns)
        {
            const float keep = g.dayT;
            auto at = [&](float d, int k) { g.dayT = d; return g.homeTarget(k); };
            const float NOON = DAY_LEN * 0.2f, DUSK = DAY_LEN * 0.44f, MIDNIGHT = DAY_LEN * 0.7f;
            check(at(MIDNIGHT, A_WOLF) >= at(NOON, A_WOLF) * 2 && at(MIDNIGHT, A_WOLF) > HOME_HERDS[A_WOLF], "wolves: at least twice as many out at night as at noon");
            check(at(NOON, A_RABBIT) > at(MIDNIGHT, A_RABBIT) * 1.5f, "rabbits: many more by day than at night");
            check(at(DUSK, A_DEER) > at(NOON, A_DEER) && at(NOON, A_DEER) > at(MIDNIGHT, A_DEER), "deer: busiest at dusk, fewer at noon, fewest at night");
            check(at(MIDNIGHT, A_BOAR) >= at(NOON, A_BOAR) && at(NOON, A_BOAR) * 10 >= HOME_HERDS[A_BOAR] * 8, "boars: out at all hours (a few more at night)");
            // where they are: each species on the ground it likes (against that ground's share of the forest)
            g.cam = V2(0, 0);
            long tiles = 0, tn[T_LAKE_MID + 1] = {};
            for (int y = 4; y < MH - 4; y++)
                for (int x = 4; x < MW - 4; x++) { tiles++; tn[g.world.tile[y][x]]++; }
            auto share = [&](float dt, int k, std::initializer_list<int> likes, double& grass, double& inGlade) {
                g.dayT = dt;
                g.animals.clear();
                for (int i = 0; i < 500; i++) g.trySpawn(k, 0, false);
                long on = 0, gr = 0, gl = 0;
                for (auto& a : g.animals) {
                    const int tx = (int)(a.pos.x / TS), ty = (int)(a.pos.y / TS);
                    for (int t : likes) on += g.world.tile[ty][tx] == t;
                    gr += g.world.tile[ty][tx] == T_GRASS;
                    for (const Glade& gl2 : g.world.glades) if (hypotf((tx - gl2.cx) / gl2.rx, (ty - gl2.cy) / gl2.ry) < 0.9f) { gl++; break; }
                }
                double terr = 0;
                for (int t : likes) terr += tn[t];
                const double n = std::max<size_t>(1, g.animals.size());
                grass = gr / n;
                inGlade = gl / n;
                return (on / n) / std::max(1e-9, terr / tiles);  // how many times more than chance they stand on the ground they like
            };
            double grDay, grNight, glA, glB, scratch;
            const double wolfLift = share(NOON, A_WOLF, {T_NEEDLES, T_ROOTS, T_DARK}, grDay, glA);
            share(MIDNIGHT, A_WOLF, {T_NEEDLES, T_ROOTS, T_DARK}, grNight, glB);
            const double boarLift = share(NOON, A_BOAR, {T_DARK, T_MOSS, T_ROOTS}, scratch, glA);
            const double rabbitLift = share(NOON, A_RABBIT, {T_GRASS, T_LEAF}, scratch, glA);
            printf("  wildlife: wolves %.1fx (needles/roots/dark), boars %.1fx (dark/moss/roots), rabbits %.1fx (grass/leaf); wolves on meadow grass: day %.0f%% night %.0f%%\n",
                   wolfLift, boarLift, rabbitLift, 100 * grDay, 100 * grNight);
            check(wolfLift > 1.5 && boarLift > 1.5 && rabbitLift > 1.05, "each species keeps to the ground it likes");
            double gd = 0, gn = 0, x1, x2;
            share(NOON, A_WOLF, {T_NEEDLES}, gd, x1);
            share(MIDNIGHT, A_WOLF, {T_NEEDLES}, gn, x2);
            check(gn > gd, "wolves range further out over the meadows at night");
            // the surplus leaves one at a time, out of sight (and never one that is near the player)
            g.animals.clear();
            g.dayT = NOON;
            for (int i = 0; i < 20; i++) {
                Animal a;
                a.kind = A_WOLF; a.biome = B_HOME; a.maxhp = a.hp = 10;
                a.pos = V2((COX + 4.f + 4 * i) * TS, (COY + 30.f) * TS);
                g.animals.push_back(a);
            }
            g.p.pos = g.animals[0].pos + V2(40, 0);
            g.cam = V2(g.p.pos.x - LW / 2, g.p.pos.y - LH / 2);
            for (auto& a : g.animals) a.target = a.pos;
            auto leavingCount = [&]() { int n = 0; for (auto& a : g.animals) n += a.leaving; return n; };
            g.cullHome(A_WOLF);
            int who = -1;
            for (size_t i = 0; i < g.animals.size(); i++) if (g.animals[i].leaving) who = (int)i;
            check(leavingCount() == 1 && who > 0 && !g.animals[0].leaving, "when the hour calls for fewer, one surplus wolf is sent home (never the one next to the player)");
            // it walks to its bed and is gone once it is there and out of sight
            g.p.hp = g.p.maxhp = 100000;
            for (int i = 0; i < 2400 && who > 0 && !g.animals[who].gone; i++) g.updateAnimals(0.1f);
            check(who > 0 && g.animals[who].gone && g.animals[who].hasBed, "...it walks to its bed and is gone once it is there and out of sight");
            g.animals.clear();
            g.dayT = keep;
        }
        // ---- wolf packs: 3-5 wolves with a leader that keep together, flank the hunter, break off sooner and scatter when the leader falls
        {
            const float keep = g.dayT;
            g.dayT = DAY_LEN * 0.7f;  // night
            g.animals.clear();
            g.nextPack = 1;
            g.p.pos = V2(6 * TS, 6 * TS);
            g.cam = V2(0, 0);
            g.populate();
            std::map<int, std::vector<int>> packs;
            int wolves = 0, lone = 0;
            for (size_t i = 0; i < g.animals.size(); i++) {
                const Animal& a = g.animals[i];
                if (a.kind != A_WOLF) continue;
                wolves++;
                if (a.pack >= 0) packs[a.pack].push_back((int)i);
                else lone++;
            }
            bool sizes = !packs.empty(), oneLeader = true, close = true;
            for (auto& kv : packs) {
                int leaders = 0, li = -1;
                for (int i : kv.second) if (g.animals[i].leader) { leaders++; li = i; }
                oneLeader = oneLeader && leaders == 1;
                sizes = sizes && kv.second.size() >= 2 && kv.second.size() <= 5;
                if (li >= 0) for (int i : kv.second) close = close && dist(g.animals[i].pos, g.animals[li].pos) < 140.f;
            }
            printf("  wolf packs at night: %zu packs, %d wolves in all (%d lone)\n", packs.size(), wolves, lone);
            check(sizes && packs.size() >= 2 && lone <= 2, "wolves come in packs of 3-5 (a lone wolf or two at most)");
            check(oneLeader && close, "each pack has exactly one leader, and the pack starts together");
            // the pack keeps together on its rounds
            g.animals.erase(std::remove_if(g.animals.begin(), g.animals.end(), [](const Animal& a) { return a.kind != A_WOLF; }), g.animals.end());
            for (auto& a : g.animals) { a.target = a.pos; a.actArrived = true; }
            for (int i = 0; i < 900; i++) g.updateAnimals(0.1f);
            double together = 0;
            int counted = 0;
            for (auto& kv : packs) {
                const Animal* L = nullptr;
                for (auto& a : g.animals) if (a.pack == kv.first && a.leader && !a.gone) L = &a;
                if (!L) continue;
                for (auto& a : g.animals) if (a.pack == kv.first && &a != L && !a.gone) { together += dist(a.pos, L->pos); counted++; }
            }
            printf("  after 90 s the followers are on average %.0f px from their leader\n", counted ? together / counted : -1.0);
            check(counted > 0 && together / counted < 220.0, "a pack keeps together: its followers stay near the leader");
            g.animals.clear();
            // a three-wolf pack in an open meadow meets the hunter: the whole pack joins the chase, and the followers come in from the sides
            if (!g.world.fields.empty()) {
                const FlowerField& f = g.world.fields[0];
                const V2 hunter(f.cx * TS, f.cy * TS);
                g.p.pos = hunter;
                g.p.walking = true; g.p.sprinting = false;
                g.p.hp = g.p.maxhp = 100000;
                g.cam = V2(0, 0);
                auto mkw = [&](V2 at, int slot, bool lead) {
                    Animal a;
                    a.kind = A_WOLF; a.biome = B_HOME; a.maxhp = a.hp = 58; a.pos = at; a.target = at;
                    a.pack = 77; a.leader = lead; a.packSlot = slot; a.flankSide = slot % 2 ? 1.f : -1.f;
                    a.face8 = F_W;
                    return a;
                };
                g.animals.push_back(mkw(hunter + V2(110, 0), 0, true));  // (inside a wolf's notice range of 125 px)
                g.animals.push_back(mkw(hunter + V2(250, 22), 1, false));
                g.animals.push_back(mkw(hunter + V2(250, -22), 2, false));
                g.animals[1].face8 = F_E;  // (the followers are not even looking at the hunter)
                g.animals[2].face8 = F_E;
                float maxSide = 0.f;
                bool allChase = false;
                for (int i = 0; i < 90; i++) {
                    g.updateAnimals(0.05f);
                    allChase = allChase || (g.animals[0].st == AS_CHASE && g.animals[1].st == AS_CHASE && g.animals[2].st == AS_CHASE);
                    // how far off the straight line from the leader's side a follower swings while closing in
                    for (int k = 1; k <= 2; k++) {
                        const V2 r = g.animals[k].pos - hunter;
                        if (r.len() > 95.f && r.len() < 260.f) maxSide = std::max(maxSide, fabsf(r.y));
                    }
                }
                check(allChase, "when one wolf sees the hunter the whole pack joins the chase (even those facing away)");
                check(maxSide > 40.f, "the followers swing out to the sides as they close in (flanking)");
                // pack wolves break off sooner than a lone wolf
                Animal lonely = mkw(hunter + V2(30, 0), 0, false);
                lonely.pack = -1; lonely.st = AS_CHASE; lonely.stT = 10.f; lonely.hp = 58 * 35 / 100;
                Animal member = mkw(hunter + V2(-30, 0), 1, false);
                member.st = AS_CHASE; member.stT = 10.f; member.hp = 58 * 35 / 100;
                g.animals.clear();
                g.animals.push_back(lonely);
                g.animals.push_back(mkw(hunter + V2(0, 300), 0, true));  // (the member's leader, far off)
                g.animals.push_back(member);
                g.updateAnimals(0.05f);
                check(g.animals[0].st == AS_CHASE && g.animals[2].st == AS_FLEE, "a pack wolf at 35% health breaks off; a lone wolf at 35% presses on");
                // the leader falls: the pack scatters, and a new leader is picked
                g.animals.clear();
                g.animals.push_back(mkw(hunter + V2(200, 0), 0, true));
                g.animals.push_back(mkw(hunter + V2(220, 20), 1, false));
                g.animals.push_back(mkw(hunter + V2(220, -20), 2, false));
                for (int i = 1; i <= 2; i++) { g.animals[i].st = AS_CHASE; g.animals[i].stT = 10.f; }
                g.killAnimal(g.animals[0], true);
                int leaders = 0;
                for (int i = 1; i <= 2; i++) leaders += g.animals[i].leader;
                check(g.animals[1].st == AS_FLEE && g.animals[2].st == AS_FLEE && leaders == 1, "when the leader falls the pack scatters, and one of the others takes over");
            }
            // the dusk howl: as the evening comes the packs call out
            g.animals.clear();
            Animal hl;
            hl.kind = A_WOLF; hl.biome = B_HOME; hl.maxhp = hl.hp = 58; hl.pos = hl.target = V2(40 * TS, 40 * TS);
            hl.pack = 5; hl.leader = true;
            g.animals.push_back(hl);
            g.dayT = DAY_LEN / 3.f;  // the late afternoon
            g.updateAnimals(0.05f);
            const int h0 = g.howls;
            g.dayT = DAY_LEN * 0.4267f;  // dusk begins
            g.updateAnimals(0.05f);
            check(g.howls > h0, "the packs howl as the evening comes");
            g.animals.clear();
            g.dayT = keep;
        }
        // ---- daily routines: where each animal is and what it is doing follows the hour; and what it can see follows its view
        {
            const float keep = g.dayT;
            const float DAWN = DAY_LEN * 0.96f, NOON = DAY_LEN * 0.2f, DUSK = DAY_LEN * 0.44f, MIDNIGHT = DAY_LEN * 0.7f;
            auto hourAt = [&](float d) { g.dayT = d; return g.hourOfDay(); };
            check(hourAt(NOON) == H_DAY && hourAt(MIDNIGHT) == H_NIGHT && hourAt(DUSK) == H_DUSK && hourAt(DAWN) == H_DAWN, "the hour of the day: dawn, day, dusk and night");
            // an open meadow spot and a lakeside spot, found on this world
            V2 meadow, shore;
            bool haveMeadow = false, haveShore = false;
            if (!g.world.fields.empty()) { meadow = V2(g.world.fields[0].cx * TS, g.world.fields[0].cy * TS); haveMeadow = true; }
            for (int y = 8; y < MH - 8 && !haveShore; y += 2)
                for (int x = 8; x < MW - 8 && !haveShore; x += 2)
                    if (g.world.tile[y][x] == T_WATER && !g.world.solidTile(x + 3, y) && g.world.tile[y][x + 3] != T_WATER && !g.world.inTown(x * TS, y * TS, 120.f)) { shore = V2((x + 3) * TS + 8.f, y * TS + 8.f); haveShore = true; }
            check(haveMeadow && haveShore, "(this world has a flower field and a lakeside to test on)");
            auto mk = [&](int kind, V2 at) {
                Animal a;
                a.kind = kind; a.biome = B_HOME; a.maxhp = a.hp = 10; a.pos = at; a.target = at;
                return a;
            };
            auto share = [&](int kind, V2 at, float day, int act, int n = 300) {
                g.dayT = day;
                Animal a = mk(kind, at);
                int hit = 0;
                for (int i = 0; i < n; i++) { g.chooseAct(a); hit += a.act == act; }
                return 100.0 * hit / n;
            };
            check(share(A_RABBIT, meadow, MIDNIGHT, ACT_SLEEP) == 100.0, "rabbits: asleep in their burrows all night");
            check(share(A_WOLF, meadow, NOON, ACT_SLEEP) >= 70.0, "wolves: sleep by their den by day");
            const double wolfNight = share(A_WOLF, meadow, MIDNIGHT, ACT_PATROL);
            check(wolfNight >= 65.0, "wolves: make their rounds at night");
            const double deerDawn = share(A_DEER, shore, DAWN, ACT_DRINK), deerMid = share(A_DEER, shore, NOON, ACT_SLEEP);
            check(deerDawn >= 15.0 && deerDawn <= 45.0 && deerMid >= 35.0, "deer: drink at dawn, lie up in the thicket at midday");
            check(share(A_BOAR, meadow, MIDNIGHT, ACT_FORAGE) >= 70.0, "boars: root about at night");
            // a wolf left to itself at noon goes to its den and sleeps there
            g.animals.clear();
            g.p.pos = V2(6 * TS, 6 * TS);
            g.dayT = NOON;
            g.animals.push_back(mk(A_WOLF, V2((COX - 30.f) * TS, (COY + 20.f) * TS)));
            bool slept = false;
            for (int i = 0; i < 3000 && !slept; i++) {
                g.updateAnimals(0.1f);
                const Animal& w = g.animals[0];
                slept = w.hasBed && w.act == ACT_SLEEP && w.st == AS_REST && dist(w.pos, w.bed) < 30.f;
            }
            check(slept, "a wolf left to itself at noon walks to its den and sleeps there");
            // what an animal sees: its view, the cover between, the noise the hunter makes
            if (haveMeadow) {
                Animal d = mk(A_DEER, meadow);
                d.face8 = F_E;
                g.dayT = NOON;
                g.p.walking = true; g.p.sprinting = false;
                g.p.pos = meadow + V2(70, 0);
                const float front = g.sightFactor(d, g.p.pos, true);
                g.p.pos = meadow - V2(70, 0);
                const float behind = g.sightFactor(d, g.p.pos, true);
                check(front >= 0.95f && behind <= 0.4f, "deer: sees a hunter in front of it at full range, one behind it only at a third");
                g.p.pos = meadow + V2(110, 0);  // (beyond the three screening tiles)
                const int tx0 = (int)(meadow.x / TS) + 2, ty0 = (int)(meadow.y / TS);
                bool saved[3];
                for (int i = 0; i < 3; i++) { saved[i] = g.world.solid[ty0][tx0 + i]; g.world.solid[ty0][tx0 + i] = true; }
                const float hidden = g.sightFactor(d, g.p.pos, true);
                g.p.sprinting = true;
                const float loud = g.sightFactor(d, g.p.pos, true);
                g.p.sprinting = false; g.p.walking = false;
                const float still = g.sightFactor(d, g.p.pos, true);
                for (int i = 0; i < 3; i++) g.world.solid[ty0][tx0 + i] = saved[i];
                check(hidden <= 0.25f && loud >= 0.45f && loud > hidden && still <= hidden + 0.001f, "three trees between hide the hunter from all but hearing; a sprint is heard through them");
                g.p.walking = false; g.p.sprinting = false;
                const float stillOpen = g.sightFactor(d, g.p.pos, true);
                check(stillOpen < front && stillOpen > 0.6f, "a hunter standing still in the open is noticed a little later");
                // end to end: the same deer, 70% of its notice range away, in front of it: it bolts in the open, and does not when screened by trees
                const float reach = ANIMAL[A_DEER].notice * TIER[0].notice;
                auto bolts = [&](bool screen) {
                    g.animals.clear();
                    Animal e = mk(A_DEER, meadow);
                    e.face8 = F_E; e.st = AS_WANDER; e.idleT = 5.f;
                    g.animals.push_back(e);
                    g.p.pos = meadow + V2(reach * 0.7f, 0);
                    g.p.walking = true; g.p.sprinting = false;
                    const int tx = (int)(meadow.x / TS) + 2, ty = (int)(meadow.y / TS);
                    bool sv[3];
                    if (screen) for (int i = 0; i < 3; i++) { sv[i] = g.world.solid[ty][tx + i]; g.world.solid[ty][tx + i] = true; }
                    for (int i = 0; i < 6; i++) g.updateAnimals(0.05f);
                    if (screen) for (int i = 0; i < 3; i++) g.world.solid[ty][tx + i] = sv[i];
                    return g.animals[0].st == AS_FLEE;
                };
                check(bolts(false) && !bolts(true), "end to end: a deer bolts from a hunter it can see, and stays calm behind a screen of trees");
            }
            g.animals.clear();
            g.dayT = keep;
        }
        g.animals.clear();
        g.trySpawn(A_RABBIT, 0, false);
        if (!g.animals.empty()) {
            Animal& a = g.animals[0];
            a.pos = V2((COX + TOWN_X0 - 7) * (float)TS, (COY + 30) * (float)TS);  // just outside the town, heading in
            bool entered = false;
            for (int i = 0; i < 300; i++) {
                g.stepAnimal(a, V2(1, 0), 60.f, 1.f / 30.f);
                entered = entered || g.world.inTown(a.pos.x, a.pos.y, 32.f);
            }
            check(!entered, "a wild animal pushed at the town edge cannot walk in");
        }
    }
    printf("town: strolling villagers, shop talk\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.updateVillagers(1.f / 30.f);
        check(g.villagers.size() == 5, "five townspeople stand in the town");
        const int tx0 = COX + TOWN_X0, ty0 = COY + TOWN_Y0, tx1 = COX + TOWN_X1, ty1 = COY + TOWN_Y1;
        auto onCobbles = [&](const Villager& v) {
            const int tx = (int)(v.pos.x / TS), ty = (int)(v.pos.y / TS);
            return tx >= tx0 && ty >= ty0 && tx < tx1 && ty < ty1 && g.world.tile[ty][tx] == T_COBBLE && !g.world.blocked(v.pos.x, v.pos.y, 4.f);
        };
        bool allOn = true;
        for (auto& v : g.villagers) allOn = allOn && onCobbles(v);
        check(allOn, "they start on open cobbles inside the town");
        std::vector<V2> start;
        for (auto& v : g.villagers) start.push_back(v.pos);
        const V2 pp = g.p.pos;
        bool stayed = true, apart = true;
        float minGap = 1e9f;
        for (int i = 0; i < 30 * 90; i++) {  // 90 seconds of strolling
            g.p.pos = pp;
            g.updateVillagers(1.f / 30.f);
            for (size_t a2 = 0; a2 < g.villagers.size(); a2++) {
                stayed = stayed && onCobbles(g.villagers[a2]);
                for (size_t b2 = a2 + 1; b2 < g.villagers.size(); b2++) minGap = std::min(minGap, dist(g.villagers[a2].pos, g.villagers[b2].pos));
            }
        }
        apart = minGap > 5.f;
        int moved = 0;
        for (size_t i = 0; i < start.size(); i++) moved += dist(start[i], g.villagers[i].pos) > 8.f;
        check(stayed, "in 90 seconds none of them leaves the cobbles or walks into a wall");
        check(moved >= 3, "most of them actually stroll somewhere");
        check(apart, "they do not walk through one another");
        // greeting the hunter
        Villager& v0 = g.villagers[0];
        g.p.pos = v0.pos + V2(18, 0);
        v0.chatCd = 0;
        v0.sayT = 0;
        g.updateVillagers(1.f / 30.f);
        check(g.villagers[0].sayT > 3.f && !g.villagers[0].moving && g.villagers[0].faceLeft == false, "a villager the hunter walks up to stops, turns and speaks");
        // not outside: nothing moves while the hunter is in a building
        std::vector<V2> before;
        for (auto& v : g.villagers) before.push_back(v.pos);
        g.enterBuilding(BLD_SMITH);
        for (int i = 0; i < 60; i++) g.updateVillagers(1.f / 30.f);
        bool still = true;
        for (size_t i = 0; i < before.size(); i++) still = still && dist(before[i], g.villagers[i].pos) < 0.01f;
        check(still, "the townspeople wait outside while the hunter is in a shop");
        // the shopkeeper greets, then chats
        g.p.pos = g.world.npcs[0].pos + V2(0, 30);
        for (int i = 0; i < 30; i++) g.updateShopTalk(1.f / 30.f);
        check(g.npcSayT > 0 && g.npcSayLine == 0, "the shopkeeper greets the hunter a moment after they come in");
        for (int i = 0; i < 30 * 14; i++) g.updateShopTalk(1.f / 30.f);
        check(g.npcSayLine != 0, "and goes on to idle remarks");
        g.exitBuilding();
        // the walk cycle is a real animation: the frames differ, and the standing frame is stable
        bool framesDiffer = true;
        for (int v = 0; v < 5; v++)
            framesDiffer = framesDiffer && mkVillager(v, 0).p != mkVillager(v, 1).p && mkVillager(v, 1).p != mkVillager(v, 3).p && mkVillager(v, 0).p == mkVillager(v, 2).p;
        check(framesDiffer, "every villager has distinct walk frames");
        check(mkNpc(BLD_SMITH, 0).p != mkNpc(BLD_SMITH, 1).p && mkNpc(BLD_FLOWER, 0).p != mkVillager(3, 0).p, "the shopkeepers still have their own look and idle breathing");
    }
    printf("town: Elder Holt\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        const V2 home((COX + 45.75f) * TS, (COY + 31.6f) * TS);
        check(dist(g.world.questGiver, home) < 1.f, "(setup) the elder starts at the spot worldgen gives him");
        g.p.pos = home + V2(260, 0);  // far away: he strolls on his own
        float far = 0;
        bool ok = true;
        for (int i = 0; i < 30 * 150; i++) {
            g.updateElder(1.f / 30.f);
            far = std::max(far, dist(g.world.questGiver, home));
            ok = ok && dist(g.world.questGiver, home) <= 30.5f && !g.world.blocked(g.world.questGiver.x, g.world.questGiver.y, 4.f);
        }
        check(far > 5.f, "in 150 seconds Elder Holt walks a few steps around his spot");
        check(ok, "he never wanders far, onto a wall or off the paving");
        g.p.pos = g.world.questGiver + V2(10, 0);
        check(g.nearQuestGiver(), "talking to him still works wherever he has strolled to");
        // he stands still for someone who has come to talk, and speaks
        const V2 at = g.world.questGiver;
        g.p.pos = at + V2(30, 0);
        g.elderChatCd = 0;
        g.questActive = false;
        for (int i = 0; i < 60; i++) g.updateElder(1.f / 30.f);
        check(dist(g.world.questGiver, at) < 0.01f && !g.elderMoving, "he stands still while the hunter is close enough to talk");
        check(g.elderSayT > 0 && std::string(g.elderSpeech()).find("HUNT") != std::string::npos, "he speaks, and offers a hunt when no quest is open");
        g.questActive = true;
        g.questProg = g.quest.goal;
        check(std::string(g.elderSpeech()).find("DONE") != std::string::npos, "his words change when a quest is ready to turn in");
        g.questActive = false;
        g.questProg = 0;
    }
    printf("HUD feedback\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.msgs.clear();
        g.msg("PACK FULL", C_RED);
        g.msg("PACK FULL", C_RED);
        g.msg("PACK FULL", C_RED);
        check(g.msgs.size() == 1 && g.msgs.back().n == 3, "the same message posted three times in a row is one line counted X3");
        g.msg("PACK FULL", C_GREEN);
        g.msg("SOMETHING ELSE", C_RED);
        check(g.msgs.size() == 3 && g.msgs[0].n == 3 && g.msgs[1].n == 1, "a different colour or text starts a new line");
        g.msgs.back().t = 0.1f;
        g.msg("SOMETHING ELSE", C_RED);
        check(g.msgs.back().t > 4.f, "a repeat refreshes the line's time on screen");
        for (int i = 0; i < 10; i++) g.msg(std::string("LINE ") + std::to_string(i), C_TEXT);
        check(g.msgs.size() <= 4, "the message area never holds more than four lines");
        g.ftexts.clear();
        g.grantXp(SK_HUNTING, 3);
        g.grantXp(SK_RANGED, 2);
        check(g.ftexts.size() >= 2 && g.ftexts[0].p.y != g.ftexts[1].p.y, "two XP gains at once are stacked, not printed on top of each other");
    }
    printf("wildlife: food chain and herd alarm\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.dayT = DAY_LEN * 0.7f;  // night: wolves hunt then (by day they sleep by their den)
        auto at = [&](V2 v) { return g.freeSpot((int)(v.x / TS), (int)(v.y / TS)); };
        const V2 far = g.p.pos + V2(420, 0);  // well outside everyone's notice range of the hunter
        // 1. a wolf stalks a rabbit, the rabbit flees, the wolf bites and kills it
        g.animals.clear();
        g.animals.reserve(16);
        int wi = (int)(g.spawnAnimal(A_WOLF, at(far), 0, 0) - &g.animals[0]);
        int ri = (int)(g.spawnAnimal(A_RABBIT, at(far + V2(50, 0)), 0, 0) - &g.animals[0]);
        g.animals[wi].biome = g.animals[ri].biome = B_HOME;
        g.animals[ri].face8 = F_W;  // (looking back towards the wolf)
        g.animals[wi].hp = g.animals[wi].maxhp = 100;
        g.animals[wi].idleT = g.animals[ri].idleT = 20.f;  // (standing about, as a routine animal does between acts: the wolf stays where the rabbit can be seen)
        bool hunted = false, fled = false, killed = false;
        const int kills0 = g.kills;
        const int64_t xp0 = g.p.sk.s[SK_HUNTING].xp, rx0 = g.p.sk.s[SK_RANGED].xp;
        for (int i = 0; i < 1800 && !killed; i++) {
            g.updateAnimals(1.f / 30.f);
            hunted = hunted || g.animals[wi].st == AS_HUNT;
            fled = fled || (g.animals[ri].st == AS_FLEE && g.animals[ri].fleeAnimal);
            killed = g.animals[ri].dead;
        }
        check(hunted, "a wolf that sees a rabbit starts to hunt it");
        check(fled, "the rabbit notices the hunting wolf and flees from it (not from the hunter)");
        check(killed && g.animals[ri].totalRemain > 0, "the wolf catches the rabbit and leaves a carcass to find");
        check(g.kills == kills0 && g.p.sk.s[SK_HUNTING].xp == xp0 && g.p.sk.s[SK_RANGED].xp == rx0, "a predator's kill gives the hunter no kill count and no XP");
        check(g.animals[wi].st == AS_FEED && g.animals[wi].huntCd > 20.f, "after the kill the wolf feeds, then rests before hunting again");
        // 2. rare specimens are never prey; other biomes' animals are not prey either
        g.animals.clear();
        wi = (int)(g.spawnAnimal(A_WOLF, at(far), 0, 0) - &g.animals[0]);
        ri = (int)(g.spawnAnimal(A_RABBIT, at(far + V2(40, 0)), 0, 2) - &g.animals[0]);
        bool everHunted = false;
        for (int i = 0; i < 600; i++) {
            g.updateAnimals(1.f / 30.f);
            everHunted = everHunted || g.animals[wi].st == AS_HUNT;
        }
        check(!everHunted && !g.animals[ri].dead, "a rare rabbit is not hunted by wolves");
        // 3. a wolf only hunts what is on its menu
        g.animals.clear();
        wi = (int)(g.spawnAnimal(A_WOLF, at(far), 0, 0) - &g.animals[0]);
        g.spawnAnimal(A_BOAR, at(far + V2(40, 0)), 0, 0);
        everHunted = false;
        for (int i = 0; i < 600; i++) {
            g.updateAnimals(1.f / 30.f);
            everHunted = everHunted || g.animals[wi].st == AS_HUNT;
        }
        check(!everHunted, "a wolf does not hunt a boar (not on its menu)");
        // 4. the hunter outranks prey: a stalking wolf turns on a hunter who comes close
        g.animals.clear();
        wi = (int)(g.spawnAnimal(A_WOLF, g.p.pos + V2(60, 0), 0, 0) - &g.animals[0]);
        g.animals[wi].st = AS_HUNT;
        g.animals[wi].stT = 8.f;
        g.animals[wi].face8 = F_W;  // (looking at the hunter, who is walking up to it)
        g.p.walking = true;
        g.updateAnimals(1.f / 30.f);
        check(g.animals[wi].st == AS_CHASE, "a hunting wolf that notices the hunter attacks the hunter instead");
        // 4b. a chase has limits: the animal gives up when its timer runs out and ignores the hunter for a while; being hit makes it bleed
        g.animals[wi].stT = 0.f;
        g.updateAnimals(1.f / 30.f);
        check(g.animals[wi].st == AS_WANDER && g.animals[wi].calmT > 0, "a wolf gives up the chase when its timer runs out");
        g.updateAnimals(1.f / 30.f);
        check(g.animals[wi].st != AS_CHASE, "a wolf that gave up does not re-chase straight away");
        g.animals[wi].st = AS_CHASE;
        g.animals[wi].chasing = true;
        g.animals[wi].chaseFrom = g.animals[wi].pos - V2(CHASE_RANGE + 40.f, 0);
        g.animals[wi].stT = 9.f;
        g.updateAnimals(1.f / 30.f);
        check(g.animals[wi].st == AS_WANDER, "a wolf gives up a chase that has run past CHASE_RANGE");
        g.animals[wi].hp = g.animals[wi].maxhp = 1000;
        g.hitAnimal(g.animals[wi], 5.f, V2(1, 0), false);
        check(g.animals[wi].bleedT > 0, "a wounded animal bleeds");
        {  // ... and the bleeding costs health, and can finish it
            Animal& bl = g.animals[wi];
            bl.hp = bl.maxhp = 100;
            bl.bleedT = 3.f;
            bl.bleedTick = 0.01f;
            g.updateAnimals(1.f / 30.f);
            check(bl.hp < 100, "a bleeding animal loses health");
            bl.hp = 1;
            bl.bleedT = 3.f;
            bl.bleedTick = 0.01f;
            g.updateAnimals(1.f / 30.f);
            check(g.animals[wi].dead, "an animal can bleed to death");
        }
        // 5. herd alarm: one deer startled by the hunter sets its neighbours running
        g.animals.clear();
        g.animals.reserve(8);
        const V2 d1 = g.freeSpot((int)((g.p.pos.x + 60) / TS), (int)(g.p.pos.y / TS));
        const float deerNotice = ANIMAL[A_DEER].notice;
        g.spawnAnimal(A_DEER, d1, 0, 0);
        g.spawnAnimal(A_DEER, d1 + V2(66, 0), 0, 0);
        g.spawnAnimal(A_DEER, d1 + V2(64, 16), 0, 0);
        for (auto& an : g.animals) an.biome = B_HOME, an.st = AS_WANDER;
        g.animals[0].face8 = F_W;  // looking at the hunter (a deer only notices what is in its view)
        const bool setupOk = dist(g.animals[0].pos, g.p.pos) < deerNotice && dist(g.animals[1].pos, g.p.pos) > deerNotice * 1.1f &&
                             dist(g.animals[2].pos, g.p.pos) > deerNotice * 1.1f && dist(g.animals[0].pos, g.animals[1].pos) < 70.f && dist(g.animals[0].pos, g.animals[2].pos) < 70.f;
        check(setupOk, "(setup) one deer sees the hunter; its two herd mates are beyond sight but within the alarm radius");
        g.updateAnimals(1.f / 30.f);
        check(g.animals[0].st == AS_FLEE, "the deer next to the hunter bolts");
        check(g.animals[1].st == AS_FLEE && g.animals[2].st == AS_FLEE && !g.animals[1].fleeAnimal, "its herd mates, who cannot see the hunter, bolt with it");
    }
    printf("audio: ambience beds and footsteps (synthesised offline, no sound device)\n");
    {
        Audio au;
        au.synth();
        bool stepsOk = true;
        for (int sfx : {(int)S_STEP_GRASS, (int)S_STEP_STONE, (int)S_STEP_WOOD, (int)S_STEP_SAND, (int)S_STEP_SNOW, (int)S_STEP_MUD}) {
            float peak = 0;
            for (float v : au.bank[sfx]) peak = std::max(peak, fabsf(v));
            stepsOk = stepsOk && !au.bank[sfx].empty() && peak > 0.05f && peak <= 1.f;
        }
        check(stepsOk, "every footstep sound exists, is audible and does not clip");
        bool bedsOk = true, distinct = true;
        double energy[AMB_COUNT] = {0};
        for (int k = AMB_DAY; k < AMB_COUNT; k++) {
            Audio b;
            b.ambNow = k;
            float peak = 0;
            bool finite = true;
            for (int i = 0; i < 44100 * 12; i++) {  // 12 seconds: long enough for a bird, frog or drip to sound
                const float v = b.ambSample();
                finite = finite && std::isfinite(v);
                peak = std::max(peak, fabsf(v));
                energy[k] += (double)v * v;
            }
            bedsOk = bedsOk && finite && peak > 0.01f && peak < 2.f;
        }
        check(bedsOk, "every ambient bed produces finite, audible, bounded sound");
        for (int a2 = AMB_DAY; a2 < AMB_COUNT; a2++)
            for (int b2 = a2 + 1; b2 < AMB_COUNT; b2++) distinct = distinct && fabs(energy[a2] - energy[b2]) > 1e-6;
        check(distinct, "the beds differ from each other (biome-specific)");
        Audio none;
        none.ambNow = AMB_NONE;
        check(none.ambSample() == 0.f, "no ambience means silence");
        // the scene choice: outdoors by biome (day/night), rooms quiet, delves cave
        const int saveBiome = g.curBiome, saveInterior = g.interior;
        const float saveDay = g.dayT;
        float vol = 0;
        g.interior = -1;
        g.curBiome = B_SNOW;
        check(g.ambienceFor(vol) == AMB_SNOW, "the snow biome has the snow-wind bed");
        g.curBiome = B_DESERT;
        check(g.ambienceFor(vol) == AMB_DESERT, "the desert has the dry-wind bed");
        g.curBiome = B_SWAMP;
        check(g.ambienceFor(vol) == AMB_SWAMP, "the swamp has the frog bed");
        g.curBiome = B_MOUNTAIN;
        check(g.ambienceFor(vol) == AMB_MOUNTAIN, "the mountains have the mountain bed");
        g.curBiome = B_HOME;
        const int homeKind = g.ambienceFor(vol);
        check(homeKind == AMB_DAY || homeKind == AMB_NIGHT, "the home meadow has a day or a night bed");
        g.interior = 0;
        check(g.ambienceFor(vol) == AMB_NONE, "inside a building the world is quiet");
        g.interior = saveInterior;
        g.curBiome = saveBiome;
        g.dayT = saveDay;
        // footsteps: the sound follows the tile under the hunter
        check(g.world.tileAt(g.p.pos.x, g.p.pos.y) < 255, "a footstep reads the tile under the hunter (no crash)");
    }
}
