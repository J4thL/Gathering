// selftest_town.cpp - `--selftest`: town NPCs, HUD feedback, wildlife and audio.
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
        auto at = [&](V2 v) { return g.freeSpot((int)(v.x / TS), (int)(v.y / TS)); };
        const V2 far = g.p.pos + V2(420, 0);  // well outside everyone's notice range of the hunter
        // 1. a wolf stalks a rabbit, the rabbit flees, the wolf bites and kills it
        g.animals.clear();
        g.animals.reserve(16);
        int wi = (int)(g.spawnAnimal(A_WOLF, at(far), 0, 0) - &g.animals[0]);
        int ri = (int)(g.spawnAnimal(A_RABBIT, at(far + V2(50, 0)), 0, 0) - &g.animals[0]);
        g.animals[wi].biome = g.animals[ri].biome = B_HOME;
        g.animals[wi].hp = g.animals[wi].maxhp = 100;
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
