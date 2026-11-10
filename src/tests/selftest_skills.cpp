// selftest_skills.cpp - `--selftest`: skill bonuses, skill XP, trophies and skills.
#include "tests/selftest_ctx.hpp"

void testSkillsAndTrophies(SelfTest& t) {
    [[maybe_unused]] Game& g = t.g;
    [[maybe_unused]] auto check = [&](bool ok, const char* what) { t.check(ok, what); };
    [[maybe_unused]] const int MEAT = t.MEAT, HIDE = t.HIDE, BONE = t.BONE;
    [[maybe_unused]] const int R_BONE = t.R_BONE, R_ARROW = t.R_ARROW, R_BOLT = t.R_BOLT, R_BANDAGE = t.R_BANDAGE;
    [[maybe_unused]] auto fillWithJunk = [&]() { t.fillWithJunk(); };
    [[maybe_unused]] auto findRecipe = [&](int kind, int out, int firstMat) { return t.findRecipe(kind, out, firstMat); };
    [[maybe_unused]] auto writeFile = [&](const std::string& text) { t.writeFile(text); };
    [[maybe_unused]] auto removeAll = [&]() { t.removeAll(); };
    printf("skill bonuses\n");
    {
        bool zero = true, mono = true, capped = true, steps = true;
        for (int k = 0; k < SK_COUNT; k++) {
            zero = zero && skillBonusAt(k, 1) == 0.f;
            capped = capped && fabsf(skillBonusAt(k, MAX_LEVEL) - SKILL[k].cap) < 1e-4f && skillBonusAt(k, 500) <= SKILL[k].cap + 1e-4f;
            for (int l = 2; l <= MAX_LEVEL; l++) mono = mono && skillBonusAt(k, l) >= skillBonusAt(k, l - 1);
            for (int m : MILESTONES) steps = steps && skillBonusAt(k, m) - skillBonusAt(k, m - 1) > SKILL[k].cap * 0.09f;  // a milestone is a visible jump
        }
        check(zero, "level 1 gives no bonus");
        check(mono, "bonuses never shrink as you level");
        check(capped, "every bonus tops out exactly at its cap at level 100 and never beyond");
        check(steps, "reaching a milestone level gives a step up in the bonus");
        g.newGame();
        g.state = ST_PLAY;
        g.p.sk.s[SK_DEFENSE].xp = SkillState::capXp();
        g.p.up[U_ARMOR] = 0;
        g.p.hp = g.p.maxhp = 100;
        g.p.invuln = 0;
        g.hurtPlayer(100, V2(0, 0), A_WOLF);
        check((int)g.p.hp == 15, "max Defense cuts incoming damage by its capped 15%");
        g.newGame();
        g.state = ST_PLAY;
        g.p.hp = 10;
        g.p.maxhp = 200;
        g.p.sk.s[SK_SURVIVAL].xp = SkillState::capXp();
        g.p.inv.add(itemCons(C_TONIC), 1);
        g.useConsumable(C_TONIC);
        check((int)g.p.hp == 10 + 117, "max Survival boosts a tonic's healing by its capped 30%");
    }
    printf("skill XP from actions\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.animals.clear();
        Animal* a = g.spawnAnimal(A_RABBIT, g.p.pos + V2(10, 0), 0, 0);
        g.killAnimal(*a);
        check(g.p.sk.s[SK_HUNTING].xp > 0 && g.p.sk.s[SK_RANGED].xp > 0, "a kill trains Hunting and Ranged");
        const int64_t easy = g.p.sk.s[SK_HUNTING].xp;
        Animal* w = g.spawnAnimal(A_WOLF, g.p.pos + V2(30, 0), 0, 0);
        const int64_t b1 = g.p.sk.s[SK_HUNTING].xp;
        g.killAnimal(*w);
        check(g.p.sk.s[SK_HUNTING].xp - b1 > easy, "a wolf is worth more Hunting XP than a rabbit");
        g.p.sk.s[SK_HUNTING].xp = SkillState::capXp() / 2;
        const int hi = g.p.sk.level(SK_HUNTING);
        check(hi > 40 && xpFalloff(hi, 10) < 1.0 && xpFalloff(hi, 10) >= 0.2 && xpFalloff(5, 10) == 1.0, "farming weak animals at a high level pays less (floor 20%)");
        g.p.dead = true;
        const int64_t bd = g.p.sk.s[SK_DEFENSE].xp;
        g.hurtPlayer(10, V2(0, 0), A_WOLF);
        check(g.p.sk.s[SK_DEFENSE].xp == bd, "no Defense XP while dead");
        g.p.dead = false;
        g.p.up[U_ARMOR] = 1;
        g.p.hp = g.p.maxhp = 100;
        g.p.invuln = 0;
        g.hurtPlayer(40, V2(0, 0), A_WOLF);
        check(g.p.sk.s[SK_DEFENSE].xp > bd, "surviving a hit in armor trains Defense");
        g.p.invuln = 0;
        g.p.hp = 5;
        const int64_t bk = g.p.sk.s[SK_DEFENSE].xp;
        g.hurtPlayer(40, V2(0, 0), A_WOLF);
        check(g.p.sk.s[SK_DEFENSE].xp == bk, "a fatal hit earns no Defense XP");
    }
    printf("trophies\n");
    {
        auto owners = [](int m) {  // how many species can drop material m (base drop, form drop or legendary drop)
            int n = 0;
            for (int k = 0; k < A_COUNT; k++)
                if (ANIMAL[k].drop[m][1] > 0 || ANIMAL[k].formDrop[0] == m || ANIMAL[k].legendDrop[0] == m) n++;
            return n;
        };
        bool oneEach = true, exclusive = true;
        for (int k = 0; k < A_COUNT; k++) {
            const int tm = M_TROPHY0 + k;
            if (ANIMAL[k].drop[tm][1] <= 0) {
                oneEach = false;
                printf("    %s has no trophy drop\n", ANIMAL[k].name);
            }
            if (owners(tm) != 1) {
                exclusive = false;
                printf("    %s is dropped by %d species\n", MAT_NAME[tm], owners(tm));
            }
        }
        check(oneEach && M_TROPHY_N == A_COUNT, "every one of the 28 species drops its own trophy");
        check(exclusive, "every trophy is dropped by exactly one species");
        bool noStray = true;  // no species drops a trophy that belongs to another
        for (int k = 0; k < A_COUNT; k++)
            for (int m = M_TROPHY0; m < M_TROPHY0 + M_TROPHY_N; m++)
                if (m != M_TROPHY0 + k && (ANIMAL[k].drop[m][1] > 0 || ANIMAL[k].formDrop[0] == m || ANIMAL[k].legendDrop[0] == m)) noStray = false;
        check(noStray, "no species drops another species' trophy");
        bool tables = true, ranges = true;
        for (int m = 0; m < M_COUNT; m++) {
            tables = tables && MAT_NAME[m] && *MAT_NAME[m] && MAT_BLURB[m] && *MAT_BLURB[m] && (MAT_PRICE[m] > 0 || m == M_CRYPTKEY); /* the crypt key is not for sale */
            for (int k = 0; k < A_COUNT; k++) ranges = ranges && ANIMAL[k].drop[m][0] >= 0 && ANIMAL[k].drop[m][0] <= ANIMAL[k].drop[m][1];
        }
        check(tables, "every material has a name, a blurb and a sell price (no table is short)");
        check(ranges, "every drop range is valid (0 <= min <= max)");
        bool permutation = true;
        int seen[M_COUNT] = {0};
        for (int i = 0; i < M_COUNT; i++) {
            if (GATHER_ORDER[i] < 0 || GATHER_ORDER[i] >= M_COUNT || seen[GATHER_ORDER[i]]++) permutation = false;
        }
        check(permutation, "the gather order lists every material exactly once");
        bool keysOk = true;  // save keys are item names: a duplicate would make two items share a key
        for (int a = 0; a < ITEM_COUNT && keysOk; a++)
            for (int b = a + 1; b < ITEM_COUNT; b++)
                if (itemKey(a) == itemKey(b)) {
                    keysOk = false;
                    printf("    duplicate item key %s\n", itemKey(a).c_str());
                    break;
                }
        check(keysOk, "item names are unique, so save keys are unique");
        bool icons = true;
        for (int m = 0; m < M_COUNT; m++) icons = icons && g.A.matIcon[m].t != nullptr;
        check(icons, "every material has an inventory icon");
        bool classes = isCarcassMat(M_MEAT) && isCarcassMat(M_CLAW) && isCarcassMat(M_WOLFFANG) && isCarcassMat(M_FAT) && !isCarcassMat(M_WOOD) && !isCarcassMat(M_GOLDBAR) &&
                       isTrophy(M_COUGARFANG) && !isTrophy(M_FAT) && !isTrophy(M_CLAW);
        check(classes, "carcass / trophy classification is right at every boundary");
        // the real thing: kill every species 60 times and look at what the carcass holds
        g.newGame();
        g.state = ST_PLAY;
        bool ownDrops = true, guaranteed = true, strayKill = true;
        for (int k = 0; k < A_COUNT; k++) {
            const int tm = M_TROPHY0 + k;
            int got = 0;
            for (int i = 0; i < 60; i++) {
                g.animals.clear();
                Animal* a = g.spawnAnimal(k, g.p.pos + V2(40, 0), 0, 0);
                g.killAnimal(*a);
                const Animal& c = g.animals[0];
                got += c.remain[tm];
                if (ANIMAL[k].drop[tm][0] >= 1 && c.remain[tm] < 1) guaranteed = false;
                for (int m = M_TROPHY0; m < M_TROPHY0 + M_TROPHY_N; m++)
                    if (m != tm && c.remain[m] > 0) strayKill = false;
            }
            if (got <= 0) {
                ownDrops = false;
                printf("    %s never dropped %s in 60 kills\n", ANIMAL[k].name, MAT_NAME[tm]);
            }
        }
        check(ownDrops, "killing each species yields its trophy");
        check(guaranteed, "species with a guaranteed trophy always carry it");
        check(strayKill, "a carcass never holds another species' trophy");
        // gathering moves trophies and fat into the pack
        g.newGame();
        g.state = ST_PLAY;
        Animal* w = g.spawnAnimal(A_WOLF, g.p.pos + V2(10, 0), 0, 0);
        g.killAnimal(*w);
        Animal& wc = *w;
        for (int i = 0; i < 100 && wc.totalRemain > 0; i++) g.gatherStep(wc);
        check(wc.totalRemain == 0 && g.p.mat(M_WOLFFANG) >= 1 && g.p.mat(M_FAT) >= 1, "gathering a wolf carcass delivers its fang and fat to the pack");
        // trophies stack, sell and save like any material
        g.p.inv.maxWeight = 1e6f;
        g.p.inv.add(itemMat(M_LIONMANE), 1200);
        check(g.p.mat(M_LIONMANE) == 1200 && g.p.inv.slots[0].n <= 999 && g.p.inv.usedSlots() >= 2, "trophies stack to 999 per slot");
        const int coinsBefore = g.p.coins;
        g.sellMat(M_LIONMANE, 20);
        check(g.p.mat(M_LIONMANE) == 1180 && g.p.coins == coinsBefore + 20 * MAT_PRICE[M_LIONMANE], "trophies sell at their listed price");
        g.p.inv.clear();
        g.p.inv.add(itemMat(M_BEARGALL), 3);
        g.p.inv.add(itemMat(M_FAT), 7);
        g.save();
        g.newGame();
        check(g.load() && g.p.mat(M_BEARGALL) == 3 && g.p.mat(M_FAT) == 7, "trophies and fat survive a save / load");
    }
    printf("skills\n");
    {
        // the curve: strictly increasing, nothing past the cap, level derived from total XP
        bool inc = true;
        for (int l = 1; l + 1 < MAX_LEVEL; l++) inc = inc && xpForLevel(l + 1) > xpForLevel(l);
        check(inc && xpForLevel(MAX_LEVEL) == 0 && xpForLevel(1) == (int64_t)XP_BASE, "XP per level rises with level and stops at the cap");
        SkillSet ss;
        check(ss.level(SK_HUNTING) == 1 && ss.add(SK_HUNTING, xpForLevel(1) - 1) == 0 && ss.level(SK_HUNTING) == 1, "one XP short of a level does not level up");
        check(ss.add(SK_HUNTING, 1) == 1 && ss.level(SK_HUNTING) == 2 && ss.s[SK_HUNTING].into() == 0, "exact XP levels up and carries zero over");
        check(ss.add(SK_HUNTING, xpForLevel(2) + xpForLevel(3) + 5) == 2 && ss.level(SK_HUNTING) == 4 && ss.s[SK_HUNTING].into() == 5, "a big award can gain several levels at once");
        check(ss.add(SK_MINING, 0) == 0 && ss.add(SK_MINING, -50) == 0 && ss.add(99, 10) == 0 && ss.add(-1, 10) == 0 && ss.s[SK_MINING].xp == 0, "zero, negative and invalid awards do nothing");
        ss.add(SK_RANGED, (int64_t)1 << 40);
        check(ss.level(SK_RANGED) == MAX_LEVEL && ss.s[SK_RANGED].xp == SkillState::capXp(), "XP is clamped at the level cap");
        bool keys = true;
        for (int a = 0; a < SK_COUNT; a++)
            for (int b = a + 1; b < SK_COUNT; b++) keys = keys && strcmp(SKILL[a].key, SKILL[b].key) != 0 && strcmp(SKILL[a].name, SKILL[b].name) != 0;
        check(keys, "skill keys and names are unique");
        // in the game: level-up reporting, dead players earn nothing
        g.newGame();
        g.state = ST_PLAY;
        check(g.grantXp(SK_WOODCUTTING, 3) == 0 && g.p.sk.s[SK_WOODCUTTING].xp == 3, "grantXp adds XP below a level-up");
        check(g.grantXp(SK_WOODCUTTING, (int)xpForLevel(1)) == 1 && g.p.sk.level(SK_WOODCUTTING) == 2, "grantXp reports a level-up");
        g.p.dead = true;
        check(g.grantXp(SK_MINING, 50) == 0 && g.p.sk.s[SK_MINING].xp == 0, "a dead hunter earns no XP");
        g.p.dead = false;
        // persistence
        g.p.sk.s[SK_COOKING].xp = 12345;
        g.p.sk.s[SK_DEFENSE].xp = 7;
        g.save();
        g.newGame();
        check(g.p.sk.s[SK_COOKING].xp == 0, "(setup) a new game starts with no skill XP");
        check(g.load() && g.p.sk.s[SK_COOKING].xp == 12345 && g.p.sk.s[SK_DEFENSE].xp == 7 && g.p.sk.s[SK_WOODCUTTING].xp > 0, "skill XP survives a save / load");
        check(g.p.sk.s[SK_MINING].xp == 0, "skills that were never trained stay at zero");
        {   // damaged / foreign lines: clamped or ignored, the rest of the save still loads
            std::ifstream in(g.savePath(), std::ios::binary);
            std::string txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();
            std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
            out << txt << "skill futureskill 99\nskill hunting -5\nskill ranged 99999999999999\nskill\nskill mining\n";
        }
        check(g.load() && g.p.sk.s[SK_HUNTING].xp >= 0 && g.p.sk.s[SK_RANGED].xp == SkillState::capXp() && g.p.sk.s[SK_COOKING].xp == 12345, "unknown, negative, oversized and truncated skill lines are handled");
        {   // a save from before skills existed has no skill lines: everything loads at level 1
            std::ifstream in(g.savePath(), std::ios::binary);
            std::string line, kept;
            while (std::getline(in, line))
                if (line.compare(0, 5, "skill") != 0) kept += line + "\n";
            in.close();
            std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
            out << kept;
        }
        bool allOne = g.load();
        for (int k = 0; k < SK_COUNT; k++) allOne = allOne && g.p.sk.level(k) == 1;
        check(allOne, "an older save without skill lines loads with every skill at level 1");
    }
    {  // ---- hunter animation: facing, clips, the state machine
        check(F_COUNT == 4 && FACING_VIEWS == 3 && AV_VIEWS == 4, "there are exactly four facings (S, E, N, W): three drawn views plus the mirror, four rendered views");
        const float X[4] = {0, 1, 0, -1}, Y[4] = {1, 0, -1, 0};  // S E N W
        bool cardinal = true;
        for (int f = 0; f < 4; f++) cardinal = cardinal && facingFromDir(X[f], Y[f]) == f;
        check(cardinal, "the four cardinal directions map to the four facings");
        bool onlyFour = true;  // every direction on the circle, in 1-degree steps, from every starting facing, gives one of the four facings
        for (int deg = 0; deg < 360; deg++)
            for (int cur = -1; cur < 4; cur++) {
                const int f = facingFromDir(cosf(deg * 0.0174533f), sinf(deg * 0.0174533f), cur);
                onlyFour = onlyFour && f >= 0 && f < F_COUNT;
            }
        check(onlyFour, "no direction ever produces a facing outside S, E, N, W");
        check(facingFromDir(1.f, 0.4f) == F_E && facingFromDir(0.4f, 1.f) == F_S && facingFromDir(-1.f, -0.4f) == F_W && facingFromDir(-0.4f, -1.f) == F_N, "a diagonal turns to face along its dominant axis");
        check(facingFromDir(1.f, 1.f) == F_E && facingFromDir(-1.f, 1.f) == F_W, "a perfect diagonal goes to the horizontal axis");
        check(facingView(F_W).view == facingView(F_E).view && facingView(F_W).mirror && !facingView(F_E).mirror, "west is the east view mirrored");
        check(facingFromDir(1.f, 1.1f, F_E) == F_E && facingFromDir(1.f, 1.5f, F_E) == F_S, "a direction wobbling near the diagonal keeps the facing (no flicker), a clear turn changes it");
        check(facingFromDir(-1.f, 1.f, F_E) == F_W, "an entity never keeps facing east while it heads west");
        {
            Animator an;
            const Clip loopC = Clip::run(0, 4, 0.1f, true), once = Clip::run(0, 3, 0.1f, false);
            an.play(1, &loopC);
            an.update(0.25f);
            const float t1 = an.t;
            an.play(1, &loopC);  // same state again: must not restart
            check(an.t == t1 && an.frame() == 2, "playing the state that is already playing does not restart it");
            an.update(0.3f);
            check(an.frame() < 4 && !an.done, "a looping clip wraps");
            an.play(2, &once);
            int firedN = 0;
            for (int i = 0; i < 20; i++) {
                an.update(0.05f);
                if (an.justDone) firedN++;
            }
            check(firedN == 1 && an.done && an.frame() == 2, "a one-shot clip finishes once and holds its last frame");
            an.play(2, &once, true);
            an.scrub(0.5f);
            check(an.frame() == 1, "scrub picks the frame by progress");
        }
        bool clipsOk = true, spritesOk = true;
        for (int st = 0; st < HS_COUNT; st++) {
            const Clip& c = hunterClip(st);
            clipsOk = clipsOk && c.n > 0 && hunterPoseCount(st) <= HS_MAXF;
            for (int i = 0; i < c.n; i++) clipsOk = clipsOk && c.frame[i] >= 0 && c.frame[i] < hunterPoseCount(st);
            for (int v = 0; v < FACING_VIEWS; v++)
                for (int i = 0; i < hunterPoseCount(st); i++) spritesOk = spritesOk && g.A.hunter[v][st][i].ok();
        }
        check(clipsOk, "every hunter clip only uses poses that exist");
        check(spritesOk, "every hunter pose is drawn in all three views (west is the east view mirrored)");
        // the state machine in play
        g.newGame();
        g.state = ST_PLAY;
        g.animals.clear();
        g.p.inv.clear();
        auto run = [&](int n) { for (int i = 0; i < n; i++) g.updatePlayer(0.02f); };
        g.in = Input();
        run(10);
        check(g.p.anim.state == HS_IDLE, "standing still plays idle");
        g.in.right = true;
        run(10);
        check(g.p.anim.state == HS_WALK && g.p.face8 == F_E, "walking right plays walk facing east");
        g.in.down = true;
        run(10);
        check(g.p.face8 == F_E, "right + down (a diagonal) still shows the east view");
        g.in.right = false;
        g.in.left = true;
        run(10);
        check(g.p.face8 == F_W && facingView(g.p.face8).mirror, "left + down (a diagonal) shows the west view (the mirrored east view)");
        g.in.sprint = true;
        run(10);
        check(g.p.anim.state == HS_RUN, "sprinting plays run");
        g.in = Input();
        run(10);
        check(g.p.anim.state == HS_IDLE && g.p.face8 == F_W, "stopping returns to idle and keeps the facing");
        g.p.invuln = 0;
        g.p.hp = g.p.maxhp;
        g.hurtPlayer(5, g.p.pos + V2(10, 0));
        run(2);
        check(g.p.anim.state == HS_HURT, "a hit plays the hurt animation");
        run(30);
        check(g.p.anim.state == HS_IDLE, "the hurt animation ends and idle resumes");
        g.p.hp = 10;
        g.p.inv.add(itemCons(C_BANDAGE), 1);
        g.useConsumable(C_BANDAGE);
        run(2);
        check(g.p.anim.state == HS_USE, "using a bandage plays the use animation");
        run(40);
        g.p.invuln = 0;
        g.p.sinceHurt = 99;
        g.p.hp = 1;
        g.hurtPlayer(50, g.p.pos + V2(10, 0));
        run(2);
        check(g.p.dead && g.p.anim.state == HS_DEATH, "being knocked out plays the death animation");
        for (int i = 0; i < 400 && g.p.dead; i++) g.updatePlayer(0.02f);
        check(!g.p.dead, "the hunter gets back up after the death animation");
        run(2);
        check(g.p.anim.state == HS_IDLE, "idle resumes after respawning");
        {  // gathering: one animation per kind of work, timed to the swing
            g.p.dead = false;
            g.p.hp = g.p.maxhp;
            g.p.impactT = 0;
            g.p.anim = Animator();
            g.updateHunterAnim(0.02f, false, false, false, true, 0, 0.f, RCL_TREE);
            check(g.p.anim.state == HS_SWING, "an axe on a tree plays the chopping stroke");
            g.updateHunterAnim(0.02f, false, false, false, true, 4, 0.f, RCL_ROCK);
            check(g.p.anim.state == HS_MINE, "a pickaxe on rock plays the mining stroke");
            g.updateHunterAnim(0.02f, false, false, false, true, 4, 0.f, RCL_ORE);
            check(g.p.anim.state == HS_MINE, "a pickaxe on ore plays the mining stroke");
            g.updateHunterAnim(0.02f, false, false, false, true, -1, 0.f, RCL_PLANT);
            check(g.p.anim.state == HS_FORAGE, "picking a plant by hand plays the foraging motion");
            g.updateHunterAnim(0.02f, false, false, true, false, -1, 0.f, -1);
            check(g.p.anim.state == HS_GATHER, "kneeling at a carcass plays the skinning motion");
            // the swing follows the harvest timer: the raise at the start, the strike just before the hit, the recovery after it
            g.updateHunterAnim(0.02f, false, false, false, true, 0, 0.f, RCL_TREE);
            check(g.p.anim.frame() == 0, "the swing starts with the raise");
            g.updateHunterAnim(0.02f, false, false, false, true, 0, 0.98f, RCL_TREE);
            check(g.p.anim.frame() == 3, "the hit lands on the impact frame");
            g.p.impactT = 0.1f;
            g.updateHunterAnim(0.02f, false, false, false, true, 0, 0.f, RCL_TREE);
            check(g.p.anim.frame() == 4, "the recovery pose follows the hit");
            g.p.impactT = 0;
            g.updateHunterAnim(0.02f, false, false, false, true, 0, 0.f, RCL_TREE);
            check(g.p.anim.frame() == 0 && g.p.anim.state == HS_SWING, "and the next swing starts again from the raise");
            g.updateHunterAnim(0.02f, false, false, false, false, -1, 0.f, -1);
            check(g.p.anim.state == HS_IDLE, "stopping returns to idle");
        }
    }
    {  // ---- the boar's full animation set
        const AnimalClips* bc = animalClips(A_BOAR);
        check(bc && animalClips(A_WOLF) && animalClips(A_DEER) && animalClips(A_RABBIT) && animalClips(A_HARE) && animalClips(A_SNOWHARE) && !animalClips(A_FENNEC) && g.A.rich[A_BOAR] && g.A.rich[A_RABBIT] && !g.A.rich[A_FENNEC], "boar, wolf, deer, rabbit and the hares have a full animation set; the others keep the walk cycle");
        check(g.A.rich[A_BOAR]->built[0] && g.A.rich[A_EAGLE]->built[0], "the common sprites of every full-set species are drawn at start-up");
        {  // rarer tiers are drawn when the first one spawns
            const int legIdx = 2 * TR_LEGENDARY;
            g.animals.clear();
            Animal* an = g.spawnAnimal(A_EAGLE, g.p.pos + V2(100, 0), 0, TR_LEGENDARY);
            check(an && g.A.rich[A_EAGLE]->built[legIdx] && g.A.rich[A_EAGLE]->s[legIdx][0][AN_FLY][0].ok(), "a legendary spawning draws its sprites on demand");
            g.animals.clear();
        }
        buildAllRichBanks(g.ren, g.A);  // the rarer tiers are drawn lazily; the completeness check wants them all
        bool bankOk = bc != nullptr, clipsOk = bc != nullptr;
        if (bc) {
            for (int kind : RICH_KINDS)
                for (int form = 0; form < ANIMAL[kind].forms; form++)
                    for (int tier = 0; tier < TR_COUNT; tier++)
                        for (int v = 0; v < AV_VIEWS; v++)
                            for (int st = 0; st < AN_COUNT; st++)
                                for (int i = 0; i < animalClips(kind)->poses[st]; i++) bankOk = bankOk && g.A.rich[kind]->s[form + 2 * tier][v][st][i].ok();
            for (int kind : RICH_KINDS) {
                const AnimalClips* kc = animalClips(kind);
                for (int st = 0; st < AN_COUNT; st++) {
                    clipsOk = clipsOk && kc->poses[st] <= AN_MAXF && (kc->poses[st] == 0) == (kc->clip[st].n == 0);  // a state has a clip exactly when it has poses
                    for (int i = 0; i < kc->clip[st].n; i++) clipsOk = clipsOk && kc->clip[st].frame[i] < kc->poses[st];
                }
            }
            clipsOk = clipsOk && bc->clip[AN_IDLE].loop && bc->clip[AN_WALK].loop && bc->clip[AN_RUN].loop && !bc->clip[AN_ATTACK].loop && !bc->clip[AN_HURT].loop && !bc->clip[AN_DEATH].loop;
        }
        check(bankOk, "every boar, wolf, deer, rabbit and hare form / rarity / view / state / pose is drawn");
        {  // birds: the flight states and the air logic
            check(animalClips(A_DUCK) && animalClips(A_EAGLE) && animalClips(A_ROADRUNNER) && g.A.rich[A_EAGLE] && animalClips(A_EAGLE)->poses[AN_FLY] == 6 && animalClips(A_EAGLE)->clip[AN_FLY].loop && !animalClips(A_EAGLE)->clip[AN_TAKEOFF].loop && !animalClips(A_EAGLE)->clip[AN_LAND].loop, "duck, eagle and roadrunner have bird sets with looping flight and one-shot takeoff / landing");
            g.animals.clear();
            g.p.pos = V2(0, 0);
            Animal d;
            d.kind = A_DUCK;
            d.hp = d.maxhp = 12;
            d.pos = d.target = V2(300, 300);
            d.bodyH = ANIMAL[A_DUCK].bodyH;
            d.st = AS_WANDER;
            g.animals.push_back(d);
            auto D_ = [&]() -> Animal& { return g.animals[0]; };
            for (int i = 0; i < 20; i++) g.updateAnimalAnim(D_(), 0.02f);
            check(D_().air == 0.f && D_().anim.state == AN_IDLE, "a resting duck stands on the ground");
            D_().vel = V2(20, 0);
            for (int i = 0; i < 5; i++) g.updateAnimalAnim(D_(), 0.02f);
            check(D_().air == 0.f && D_().anim.state == AN_WALK, "a wandering duck walks");
            D_().st = AS_FLEE;
            D_().vel = V2(100, 0);
            g.updateAnimalAnim(D_(), 0.02f);
            check(D_().anim.state == AN_TAKEOFF && D_().air > 0.f, "a fleeing duck takes off");
            float prevAir = D_().air;
            bool rises = true;
            for (int i = 0; i < 12; i++) {
                g.updateAnimalAnim(D_(), 0.02f);
                rises = rises && D_().air >= prevAir;
                prevAir = D_().air;
            }
            check(rises, "it climbs smoothly");
            for (int i = 0; i < 60; i++) g.updateAnimalAnim(D_(), 0.02f);
            check(D_().air == FLY_HEIGHT && D_().anim.state == AN_FLY, "then cruises with the wing-beat cycle");
            D_().st = AS_WANDER;
            D_().vel = V2();
            g.updateAnimalAnim(D_(), 0.02f);
            check(D_().anim.state == AN_LAND, "once calm it lands");
            for (int i = 0; i < 80; i++) g.updateAnimalAnim(D_(), 0.02f);
            check(D_().air == 0.f && D_().anim.state == AN_IDLE, "and is back to standing on the ground");
            D_().st = AS_FLEE;
            for (int i = 0; i < 80; i++) g.updateAnimalAnim(D_(), 0.02f);
            g.hitAnimal(D_(), 1.f, V2(1, 0), false);
            g.updateAnimalAnim(D_(), 0.02f);
            check(D_().anim.state == AN_HURT && D_().air > 0.f, "a hit flyer flinches in the air");
            g.animals[0].kind = A_EAGLE;
            g.animals[0].st = AS_WANDER;
            g.animals[0].vel = V2(30, 0);
            g.animals[0].hurtT = 0;
            for (int i = 0; i < 80; i++) g.updateAnimalAnim(D_(), 0.02f);
            check(D_().anim.state == AN_FLY && D_().air == FLY_HEIGHT, "a soaring eagle stays aloft while it moves");
            D_().st = AS_REST;
            D_().vel = V2();
            for (int i = 0; i < 80; i++) g.updateAnimalAnim(D_(), 0.02f);
            check(D_().air == 0.f, "and lands to rest");
            g.animals.clear();
        }
        {  // the exporter: PNG sheets for all eight directions of every state, plus JSON metadata
            const std::string dir = (std::filesystem::temp_directory_path() / "wildwood_export_test").string();
            std::filesystem::remove_all(dir);
            const int n = sprexp::exportAll(dir);
            check(n > 0, "the sprite exporter writes its files");
            int pngs = 0, bad = 0;
            for (auto& e : std::filesystem::recursive_directory_iterator(dir))
                if (e.path().extension() == ".png") {
                    pngs++;
                    FILE* f = fopen(e.path().string().c_str(), "rb");
                    unsigned char h[24] = {0};
                    if (!f || fread(h, 1, 24, f) != 24 || memcmp(h, "\x89PNG\r\n\x1a\n", 8) != 0 || ((h[16] << 24) | (h[17] << 16) | (h[18] << 8) | h[19]) < 8) bad++;
                    if (f) fclose(f);
                }
            int expect = 0;
            for (int st = 0; st < HS_COUNT; st++) expect += F_COUNT;
            for (int kind : RICH_KINDS)
                for (int form = 0; form < ANIMAL[kind].forms; form++)
                    for (int st = 0; st < AN_COUNT; st++)
                        if (animalClips(kind)->poses[st] > 0) expect += F_COUNT;
            check(pngs == expect && bad == 0, "one valid PNG per state and direction (4 directions each)");
            check(std::filesystem::exists(dir + "/sprites/animals/wild_duck/wild_duck_fly_west.png") && std::filesystem::exists(dir + "/animations/animals/deer_walk.json") &&
                      std::filesystem::exists(dir + "/sprites/player/gathering/chopping/player_swing_east.png") && std::filesystem::exists(dir + "/animations/manifest.json"),
                  "files are named species_state_direction and have metadata");
            std::filesystem::remove_all(dir);
        }
        {  // calm states (graze, rest) and the startle before a prey animal bolts
            g.animals.clear();
            g.p.dead = false;
            Animal d;
            d.kind = A_DEER;
            d.hp = d.maxhp = ANIMAL[A_DEER].hp;
            d.bodyH = ANIMAL[A_DEER].bodyH;
            d.pos = d.target = g.p.pos + V2(400, 0);
            d.st = AS_FEED;
            g.animals.push_back(d);
            auto E_ = [&]() -> Animal& { return g.animals[0]; };
            g.updateAnimalAnim(E_(), 0.02f);
            check(E_().anim.state == AN_EAT, "a grazing animal plays the eating animation");
            E_().st = AS_REST;
            g.updateAnimalAnim(E_(), 0.02f);
            check(E_().anim.state == AN_REST, "a resting animal lies down");
            bool poses = true;
            for (int kind : RICH_KINDS)
                for (int st : {AN_EAT, AN_REST, AN_ALERT}) poses = poses && animalClips(kind)->poses[st] > 0 && animalClips(kind)->clip[st].n > 0;
            check(poses, "every species with a full set can eat, rest and be alert");
            // try a few spots around the hunter (the first may be walled in): the first one it can run from is checked
            bool startled = false, froze = true, bolted = false, dust = false;
            for (const V2 off : {V2(40, 0), V2(-40, 0), V2(0, 40), V2(0, -40), V2(60, 60), V2(-60, -60)}) {
                if (bolted) break;
                E_().st = AS_WANDER;
                E_().alertT = 0;
                E_().vel = V2();
                E_().pos = E_().target = g.p.pos + off;
                E_().anim = Animator();
                const V2 p0 = E_().pos;
                g.updateAnimals(0.02f);
                startled = E_().st == AS_FLEE && E_().alertT > 0 && E_().anim.state == AN_ALERT;
                if (!startled) continue;
                for (int i = 0; i < 10; i++) g.updateAnimals(0.02f);
                froze = dist(p0, E_().pos) < 0.5f && E_().anim.state == AN_ALERT;
                for (int i = 0; i < 60; i++) g.updateAnimals(0.02f);
                bolted = E_().alertT <= 0 && dist(p0, E_().pos) > 4.f && E_().anim.state == AN_RUN;
                if (bolted) {  // a running animal kicks up dust
                    g.parts.clear();
                    for (int i = 0; i < 150; i++) g.updateAnimals(0.02f);
                    dust = !g.parts.empty();
                }
            }
            check(dust, "a running animal kicks up dust");
            check(startled, "a deer that notices the hunter is startled first");
            check(froze, "it freezes and looks while the alert plays");
            check(bolted, "then it bolts");
            g.animals.clear();
        }
        check(clipsOk, "every species' clips only use poses that exist; the boar's idle, walk and run loop, attack, hurt and death play once");
        if (bc) {
            g.newGame();
            g.state = ST_PLAY;
            g.animals.clear();
            Animal* a = g.spawnAnimal(A_BOAR, g.p.pos + V2(80, 0), 0, 0);
            check(a != nullptr, "test setup: a boar");
            if (a) {
                const int idx = (int)(a - &g.animals[0]);
                auto A_ = [&]() -> Animal& { return g.animals[idx]; };
                A_().hp = A_().maxhp = 5000;
                A_().st = AS_WANDER;
                A_().vel = V2();
                g.updateAnimalAnim(A_(), 0.02f);
                check(A_().anim.state == AN_IDLE, "a standing boar plays idle");
                A_().vel = V2(20, 0);
                g.updateAnimalAnim(A_(), 0.02f);
                check(A_().anim.state == AN_WALK && A_().face8 == F_E, "a wandering boar plays walk facing the way it moves");
                const float t1 = A_().anim.t;
                g.updateAnimalAnim(A_(), 0.02f);
                check(A_().anim.t > t1, "walking keeps advancing (no restart every frame)");
                A_().vel = V2(-15, 15);
                g.updateAnimalAnim(A_(), 0.02f);
                check(A_().face8 == F_W, "moving down-left (a diagonal) shows the west view");
                A_().st = AS_FLEE;
                A_().vel = V2(60, 0);
                g.updateAnimalAnim(A_(), 0.02f);
                check(A_().anim.state == AN_RUN, "a fleeing boar plays run");
                // a real bite: put a charging boar next to the hunter
                g.p.invuln = 0;
                g.p.hp = g.p.maxhp;
                A_().st = AS_CHASE;
                A_().atkCd = 0;
                A_().pos = g.p.pos + V2(8, 0);
                g.updateAnimals(0.02f);
                check(A_().atkT > 0 && A_().anim.state == AN_ATTACK && g.p.hp < g.p.maxhp, "a charging boar bites: the attack animation plays as the hit lands");
                check(A_().face8 == F_W, "the boar faces the hunter while it attacks");
                for (int i = 0; i < 40; i++) g.updateAnimalAnim(A_(), 0.02f);
                check(A_().anim.state != AN_ATTACK, "the attack animation ends");
                g.hitAnimal(A_(), 1.f, V2(1, 0), false);
                g.updateAnimalAnim(A_(), 0.02f);
                check(A_().anim.state == AN_HURT, "a hit plays the hurt animation");
                g.killAnimal(A_());
                for (int i = 0; i < 80; i++) g.updateAnimalAnim(A_(), 0.02f);
                check(A_().dead && A_().anim.state == AN_DEATH && A_().anim.done && A_().anim.frame() == bc->poses[AN_DEATH] - 1, "a dead boar plays death once and stays on the last pose");
                {  // the death slide: momentum carries the body a short way, slows smoothly and stops; the corpse stays
                    Animal& b = A_();
                    const V2 p0 = b.pos;
                    const float sp0 = b.kb.len();
                    float prev = sp0, spMid = 0;
                    bool mono = true;
                    for (int i = 0; i < 300 && b.kb.len() > 0; i++) {
                        g.updateAnimals(0.02f);
                        const float s = b.kb.len();
                        mono = mono && s <= prev + 1e-3f;
                        prev = s;
                        if (i == 10) spMid = s;
                    }
                    const float d = dist(p0, b.pos);
                    check(sp0 > 50.f, "the killing blow starts a slide");
                    check(d > 6.f && d < 60.f, "the corpse slides a short distance");
                    check(mono && spMid > 0.f && spMid < sp0, "the slide slows down smoothly");
                    check(b.kb.len() == 0.f, "the slide comes to a complete stop");
                    const V2 pStop = b.pos;
                    for (int i = 0; i < 50; i++) g.updateAnimals(0.02f);
                    check(dist(pStop, b.pos) == 0.f && !b.gone && b.totalRemain > 0 && b.anim.state == AN_DEATH && b.anim.done, "the stopped corpse stays put, gatherable, on its last death pose");
                }
            }
        }
    }
}
