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
        // perks: a pair of choices at each milestone, final, saved, and really applied
        {
            g.p.dead = false;
            check(!g.p.sk.canChoose(SK_HUNTING, 0) && !g.p.sk.choose(SK_HUNTING, 0, 0), "a perk is closed until its milestone level");
            g.p.sk.s[SK_HUNTING].xp = 80000;  // level 50: three steps open
            g.p.sk.s[SK_WOODCUTTING].xp = 80000;
            check(g.p.sk.pending(SK_HUNTING) == 3 && !g.p.sk.canChoose(SK_HUNTING, 3), "the steps up to the hunter's level are open");
            const float base = g.skillBonus(SK_HUNTING);
            check(g.p.sk.choose(SK_HUNTING, 0, 0) && !g.p.sk.choose(SK_HUNTING, 0, 1) && !g.p.sk.choose(SK_HUNTING, 1, 2), "a perk is chosen once and only with a valid option");
            check(fabsf(g.skillBonus(SK_HUNTING) - base - PERK_MASTERY_SHARE[0] * SKILL[SK_HUNTING].cap) < 1e-4f, "a MASTERY perk adds its share of the cap to the bonus");
            const int before = g.p.sk.level(SK_HUNTING);
            g.p.sk.choose(SK_HUNTING, 1, 1);
            const int64_t xp0 = g.p.sk.s[SK_HUNTING].xp;
            g.grantXp(SK_HUNTING, 100);
            check(g.p.sk.s[SK_HUNTING].xp - xp0 == 115 && g.p.sk.level(SK_HUNTING) >= before, "a QUICK STUDY perk pays its extra XP");
            g.p.sk.choose(SK_WOODCUTTING, 0, 1);
            check(fabsf(g.p.sk.perkSpeed(SK_WOODCUTTING) - PERK_B_VALUE[0]) < 1e-6f && g.p.sk.perkLearn(SK_WOODCUTTING) == 0.f, "woodcutting's second perk is speed, not XP");
            g.save();
            g.newGame();
            check(g.p.sk.perk[SK_HUNTING][0] == -1, "(setup) a new game starts with no perks");
            check(g.load() && g.p.sk.perk[SK_HUNTING][0] == 0 && g.p.sk.perk[SK_HUNTING][1] == 1 && g.p.sk.perk[SK_HUNTING][2] == -1 && g.p.sk.perk[SK_WOODCUTTING][0] == 1, "perks survive a save / load");
        }
        // hunter rank: from the sum of skill levels, with a real carry bonus
        {
            const SkillSet keep = g.p.sk;
            SkillSet fresh;
            check(fresh.totalLevels() == SK_COUNT && fresh.rank() == 0, "a new hunter is an Apprentice");
            const int limit0 = g.p.carryLimit() - RANK_CARRY_KG * g.p.sk.rank();
            for (int k = 0; k < SK_COUNT; k++) g.p.sk.s[k].xp = SkillState::capXp();
            check(g.p.sk.totalLevels() == SK_COUNT * MAX_LEVEL && g.p.sk.rank() == RANK_COUNT - 1, "all skills maxed is the top rank");
            g.p.syncPack();
            check(g.p.carryLimit() == limit0 + RANK_CARRY_KG * (RANK_COUNT - 1) && (int)g.p.inv.maxWeight == g.p.carryLimit(), "each rank raises the carry limit");
            g.p.sk = keep;
            g.p.syncPack();
        }
        // trophies: read from the bestiary, paid once, saved
        {
            const SkillSet keepSk = g.p.sk;
            g.newGame();
            g.p.dead = false;
            const int c0 = g.p.coins;
            for (int i = 0; i < 10; i++) g.bestKills[i] = 1;  // ten species found (set directly, as an old save would)
            g.bestiaryKill(A_RABBIT);
            check(g.trophyMask == 1 && g.p.coins - c0 >= 100, "ten species found pays the NATURALIST trophy");
            const int c1 = g.p.coins;
            g.bestiaryKill(A_RABBIT);
            check(g.p.coins == c1 && g.trophyMask == 1, "a trophy is paid only once");
            g.bestiaryKill(A_WOLF, TR_LEGENDARY);
            check(g.trophyMask & (1 << 3), "a legendary kill earns LEGEND SEEKER");
            g.save();
            const int m = g.trophyMask;
            g.newGame();
            check(g.trophyMask == 0 && g.load() && g.trophyMask == m, "paid trophies survive a save / load");
            g.p.sk = keepSk;
            g.p.dead = false;
        }
        // daily hunt trial: a kill goal paid once, saved with the game
        {
            const SkillSet keepSk = g.p.sk;
            g.newGame();
            g.p.dead = false;
            g.p.sk = SkillSet();
            g.makeTrial(4);
            check(g.trial.kind == 0 && g.trial.need >= 3 && g.trial.pay > 0 && !g.trial.done, "a fresh hunter's trial is a species hunt");
            const int sp = g.trial.species;
            g.bestiaryKill(sp == A_WOLF ? A_RABBIT : A_WOLF);  // some other species does not count
            check(g.trial.got == 0 || sp == A_WOLF || sp == A_RABBIT, "(setup) other species leave the count alone");
            g.trial.got = 0;
            for (int i = 0; i < g.trial.need - 1; i++) g.bestiaryKill(sp);
            check(!g.trial.done && g.trial.got == g.trial.need - 1, "the trial counts only its species");
            g.kingTier = 3;
            g.save();
            g.newGame();
            check(g.load() && g.trial.species == sp && g.trial.got == g.trial.need - 1, "trial progress survives a save / load");
            check(g.kingTier == 3, "the Frostbound King's tier survives a save / load");
            const int c1 = g.p.coins;
            g.bestiaryKill(sp);
            check(g.trial.done && g.p.coins - c1 >= g.trial.pay, "finishing the trial pays it");
            const int c2 = g.p.coins;
            g.bestiaryKill(sp);
            check(g.p.coins == c2 && g.trial.got == g.trial.need, "it pays only once");
            for (int k = 0; k < SK_COUNT; k++) g.p.sk.s[k].xp = 400000;
            g.makeTrial(3);
            check(g.trial.kind == 1, "Warden and up get a legendary hunt every third day");
            g.bestiaryKill(A_DEER, TR_COMMON);
            check(!g.trial.done, "a common kill does not finish a legendary hunt");
            g.bestiaryKill(A_DEER, TR_LEGENDARY);
            check(g.trial.done, "a legendary kill does");
            // fishing: trophies for the catch, and a fishing goal every fourth day for a hunter with a rod
            g.fishCaught = 10;
            g.checkTrophies();
            check(g.trophyMask >> 9 & 1 && !(g.trophyMask >> 10 & 1), "ten fish caught earn the ANGLER trophy");
            check(g.trophyProgress(11) == 10, "(setup) the fish trophies count the catch");
            g.makeTrial(5);
            check(g.trial.kind == 0, "without a rod there is no fishing goal");
            g.p.inv.add(itemTool(TL_ROD), 1);
            g.makeTrial(5);
            check(g.trial.kind == 2 && g.trial.need >= 3 && g.trialLabel().rfind("CATCH ", 0) == 0, "with a rod every fourth day's goal is to catch fish");
            const int kc = g.p.coins;
            g.bestiaryKill(A_RABBIT);
            check(g.trial.got == 0, "kills do not count toward a fishing goal");
            g.trialFish(g.trial.need);
            check(g.trial.done && g.p.coins - kc >= g.trial.pay, "catching enough fish pays the trial");
            g.p.inv.remove(itemTool(TL_ROD), 1);
            g.p.sk = keepSk;
            g.p.dead = false;
        }
        // the journal tab draws in every state (no quest, a finished trial, all trophies paid)
        {
            const int tab = g.invTab;
            g.invTab = 4;
            g.state = ST_INVENTORY;
            g.render();
            g.trial.done = true;
            g.trophyMask = (1 << TROPHY_COUNT) - 1;
            g.render();
            check(g.state == ST_INVENTORY, "the journal tab renders");
            g.trophyMask = 0;
            g.trial.done = false;
            g.state = ST_PLAY;
            g.invTab = tab;
        }
        // seasons: a week each, spring first; they change the herds, storms and what a kill gives
        {
            const float day0 = g.dayT;
            g.dayT = 0.f;
            check(g.season() == SEASON_SPRING && g.seasonHerds() > 1.f && g.seasonStorm() < 1.f, "the year starts in a wet, lively spring");
            g.dayT = SEASON_DAYS * 300.f + 1.f;
            check(g.season() == SEASON_SUMMER && g.seasonStorm() > 1.f, "a week later it is summer: fewer storms");
            g.dayT = 3 * SEASON_DAYS * 300.f + 1.f;
            check(g.season() == SEASON_WINTER && g.seasonHerds() < 1.f, "winter thins the herds");
            g.dayT = 4 * SEASON_DAYS * 300.f + 1.f;
            check(g.season() == SEASON_SPRING, "and the year turns round to spring");
            // thick winter coats and fat autumn game
            auto yield = [&](float dayT, int mat) {
                g.dayT = dayT;
                g.animals.clear();
                Animal* a = g.spawnAnimal(A_DEER, g.p.pos + V2(60, 0), 0, 0);
                if (!a) return -1;
                g.killAnimal(*a, true);
                return (int)a->remain[mat];
            };
            const int hideSpring = yield(0.f, M_HIDE), hideWinter = yield(3 * SEASON_DAYS * 300.f + 1.f, M_HIDE);
            const int meatSpring = yield(0.f, M_MEAT), meatAutumn = yield(2 * SEASON_DAYS * 300.f + 1.f, M_MEAT);
            check(hideSpring >= 0 && hideWinter >= ANIMAL[A_DEER].drop[M_HIDE][0] + 1 && meatAutumn >= ANIMAL[A_DEER].drop[M_MEAT][0] + 1 && meatSpring >= 0, "a winter kill gives an extra hide and an autumn kill extra meat");
            g.animals.clear();
            g.dayT = day0;
        }
        // fishing: cast at water, wait for the bite, click in time; fish, XP and the count are real
        {
            const SkillSet keepSk = g.p.sk;
            const bool hadDead = g.p.dead;
            g.p.dead = false;
            g.state = ST_PLAY;
            int wx = -1, wy = -1;
            for (int y = 3; y < MH - 3 && wx < 0; y++)
                for (int x = 3; x < MW - 3 && wx < 0; x++)
                    if (g.world.water(x, y) && !g.world.water(x - 1, y) && !g.world.blocked((x - 1.5f) * TS, (y + .5f) * TS, 4)) wx = x, wy = y;
            check(wx >= 0, "(setup) the world has water to fish in");
            if (wx >= 0) {
                g.p.pos = V2((wx - 1.5f) * TS, (wy + .5f) * TS);
                g.setCam(1.f);
                g.p.inv.add(itemTool(TL_ROD), 1);
                g.equipHotQuiet(itemTool(TL_ROD));
                g.hotSelect(g.p.hotSlotOf(itemTool(TL_ROD)), false);
                check(g.p.hand == HAND_ITEM, "(setup) the rod is in hand");
                g.in.mx = (int)((wx + .5f) * TS) - g.camX;
                g.in.my = (int)((wy + .5f) * TS) - g.camY;
                const int fish0 = g.fishCaught;
                const int64_t xp0 = g.p.sk.s[SK_FISHING].xp;
                g.fishClick();
                check(g.fishing.phase == 1, "a cast at water starts the wait");
                g.fishClick();
                check(g.fishing.phase == 0 && g.fishCaught == fish0, "reeling in before a bite catches nothing");
                g.fishClick();
                for (int i = 0; i < 1200 && g.fishing.phase == 1; i++) g.updateFishing(1.f / 60.f);
                check(g.fishing.phase == 2, "after a few seconds a fish bites");
                g.fishClick();
                int fishItems = 0;
                for (int m = M_TROUT; m <= M_PIKE; m++) fishItems += g.p.inv.count(itemMat(m));
                check(g.fishing.phase == 0 && g.fishCaught > fish0 && fishItems == g.fishCaught - fish0 && g.p.sk.s[SK_FISHING].xp > xp0, "clicking in time lands a fish, pays XP and counts it");
                g.fishClick();
                for (int i = 0; i < 1200 && g.fishing.phase == 1; i++) g.updateFishing(1.f / 60.f);
                for (int i = 0; i < 120 && g.fishing.phase == 2; i++) g.updateFishing(1.f / 60.f);
                check(g.fishing.phase == 0, "a late click lets the fish go (the bite window closes)");
                g.in.mx = g.in.my = 0;
                g.fishClick();
                check(g.fishing.phase == 0 || g.world.water((int)((g.in.mx + g.camX) / TS), (int)((g.in.my + g.camY) / TS)), "casting away from water does nothing");
                g.fishing = Game::Fishing();
            }
            g.p.sk = keepSk;
            g.p.dead = hadDead;
        }
        // the companion: a grown mount that follows outside, earns bond from kills and digs up small finds
        {
            const SkillSet keepSk = g.p.sk;
            g.newGame();
            g.p.dead = false;
            g.state = ST_PLAY;
            g.hsAddAnimal(DA_MOUNT, g.p.pos, false, -1, A_WOLF, 1);
            check(!g.hs.animals.empty() && g.companionAnimal() == nullptr, "a mount is not a companion until chosen");
            HAnimal& m = g.hs.animals.back();
            m.age = 1e6f;  // grown
            m.companion = true;
            check(g.companionAnimal() == &m, "a grown mount can be the companion");
            check(g.companionDamage() == 0.f, "at home it adds no damage (it is not with the hunter)");
            g.updateCompanion(0.1f, true);
            check(g.companion.active && m.away && g.companionDamage() > 0.f, "outside it joins the hunter and adds damage");
            const V2 start = g.companion.pos;
            g.p.pos += V2(60, 0);
            for (int i = 0; i < 90; i++) g.updateCompanion(1.f / 30.f, true);
            check(dist(g.companion.pos, g.p.pos) < 40.f && dist(g.companion.pos, start) > 20.f, "it follows the hunter");
            const int b0 = m.bond;
            for (int i = 0; i < COMPANION_BOND_AT[1]; i++) g.companionKill();
            check(m.bond == b0 + COMPANION_BOND_AT[1] && Game::bondLevel(m.bond) == 1 && g.companionDamage() > COMPANION_DAMAGE_PER_LEVEL, "kills with it close by build the bond and raise the damage");
            const size_t items0 = g.items.size();
            g.companion.forageT = 0.f;
            g.updateCompanion(0.1f, true);
            check(g.items.size() == items0 + 1, "now and then it digs up a small find");
            g.save();
            g.newGame();
            check(g.load() && g.hs.animals.size() == 1 && g.hs.animals[0].companion && g.hs.animals[0].bond == COMPANION_BOND_AT[1] && !g.hs.animals[0].away, "the companion and its bond survive a save / load (and it is home again)");
            g.updateCompanion(0.1f, true);
            check(g.companion.active, "(setup) it sets out again");
            g.companionHome();
            check(!g.companion.active && !g.hs.animals[0].away, "going home takes it off the hunter's side and back to the pen");
            g.hs.animals[0].companion = false;
            g.updateCompanion(0.1f, true);
            check(!g.companion.active, "with no companion chosen, nothing follows");
            g.p.sk = keepSk;
            g.p.dead = false;
        }
        // companion orders: fetch thrown weapons, scout for game; saddlebags; breeding mounts
        {
            const SkillSet keepSk = g.p.sk;
            g.newGame();
            g.p.dead = false;
            g.state = ST_PLAY;
            g.hsAddAnimal(DA_MOUNT, g.p.pos, false, -1, A_WOLF, 1);
            HAnimal& m = g.hs.animals.back();
            m.age = 1e6f;
            m.companion = true;
            g.updateCompanion(0.1f, true);
            check(g.companion.active && g.companion.mode == Game::CM_FOLLOW, "(setup) the companion is out and follows by default");
            g.companionOrder();
            check(g.companion.mode == Game::CM_FETCH, "the order key switches follow -> fetch");
            g.companionOrder();
            check(g.companion.mode == Game::CM_FOLLOW, "scouting is skipped until the bond reaches level 2 (back to follow)");
            m.bond = COMPANION_BOND_AT[1];
            g.companionOrder(); g.companionOrder();
            check(g.companion.mode == Game::CM_SCOUT, "with the bond it can scout");
            g.companionOrder();
            check(g.companion.mode == Game::CM_FOLLOW, "...and the cycle returns to follow");
            // fetch
            g.companion.mode = Game::CM_FETCH;
            GroundItem gi{};
            V2 way(110, 0);  // a direction with open ground all the way (the forest has trees and rocks)
            for (const V2& cand : {V2(110, 0), V2(-110, 0), V2(0, 110), V2(0, -110), V2(78, 78), V2(-78, 78), V2(78, -78), V2(-78, -78)}) {
                bool clear = true;
                for (int k = 0; k <= 22; k++) clear = clear && !g.world.blocked(g.p.pos.x + cand.x * k / 22.f, g.p.pos.y + cand.y * k / 22.f, 5.f);
                if (clear) { way = cand; break; }
            }
            gi.pos = g.p.pos + way;
            gi.wpn = W_JAVELIN; gi.ammo = -1; gi.item = -1;
            g.items.clear();
            g.items.push_back(gi);
            const V2 thrownAt = g.items[0].pos;
            for (int i = 0; i < 400 && dist(g.items[0].pos, g.p.pos) > 30.f; i++) g.updateCompanion(1.f / 30.f, true);
            check(dist(thrownAt, g.items[0].pos) > 40.f && dist(g.items[0].pos, g.p.pos) < 30.f, "fetch: the companion brings a thrown weapon back to the hunter");
            GroundItem dropped{};
            dropped.pos = g.p.pos + way;
            dropped.item = itemMat(M_MEAT); dropped.n = 1; dropped.ammo = -1;
            g.items.clear(); g.items.push_back(dropped);
            for (int i = 0; i < 200; i++) g.updateCompanion(1.f / 30.f, true);
            check(dist(g.items[0].pos, dropped.pos) < 1.f, "fetch leaves stacks you dropped on purpose alone");
            // scout
            g.companion.mode = Game::CM_SCOUT;
            g.companion.orderT = 0.f;
            g.animals.clear();
            g.spawnAnimal(A_DEER, g.p.pos + V2(90, 0), 0, 0);
            g.spawnAnimal(A_DEER, g.p.pos + V2(900, 0), 0, 0);
            g.updateCompanion(0.1f, true);
            check(g.animals.size() == 2 && g.animals[0].scoutT > 0.f && g.animals[1].scoutT == 0.f, "scout: animals close by are marked on the map, far ones are not");
            // saddlebags
            m.companion = false;
            m.saddled = true;
            g.updateCompanion(0.1f, true);
            const int base = mountCarry(A_WOLF, 1);
            check(g.mountKgNow(m) == base, "(setup) a mount's own carry weight without saddlebags");
            g.p.inv.add(itemCarry(CG_SADDLEBAG), 1);
            check(g.mountKgNow(m) == (int)lroundf(base * 1.5f), "saddlebags in the pack add half again to the mount's carry weight");
            g.p.inv.remove(itemCarry(CG_SADDLEBAG), 1);
            // breeding
            g.hs.animals.clear();
            g.hsAddAnimal(DA_MOUNT, V2(500, 500), false, -1, A_WOLF, 1);
            g.hsAddAnimal(DA_MOUNT, V2(510, 500), false, -1, A_WOLF, 2);
            g.hsAddAnimal(DA_MOUNT, V2(520, 500), false, -1, A_DEER, 3);
            for (auto& h : g.hs.animals) { h.age = 1e6f; h.full = h.happy = h.health = 100.f; h.breedCd = 0.f; }
            g.hs.breedT = 10.f;
            g.hsTickAnimals(0.1f);
            check(g.hs.animals.size() == 4, "two grown mounts of one species breed: a baby is born");
            const HAnimal& baby = g.hs.animals.back();
            check(baby.kind == DA_MOUNT && baby.species == A_WOLF && !baby.adult(DOMS[DA_MOUNT]) && baby.tier >= 2, "the baby is of their species and takes the better parent's tier");
            check(g.hs.animals[0].breedCd >= BREED_CD * 1.5f, "mounts breed only now and then (a long wait before the next foal)");
            g.p.sk = keepSk;
            g.p.dead = false;
        }
        // dodge-roll: a burst with i-frames that costs stamina, has a cooldown, and is refused when exhausted or riding
        {
            const SkillSet keepSk = g.p.sk;
            g.newGame();
            g.p.dead = false;
            g.state = ST_PLAY;
            g.animals.clear();
            g.in = Input();
            g.in.right = true;
            g.p.stamina = 100.f;
            g.p.invuln = 0.f;
            const V2 start = g.p.pos;
            g.in.dodgePressed = true;
            g.updatePlayer(0.016f);
            g.in.dodgePressed = false;
            check(g.p.rollT > 0 && g.p.invuln >= DODGE_TIME && g.p.stamina <= 100.f - DODGE_COST + 1.f, "the dodge key starts a roll: i-frames and a stamina cost");
            const int hp0 = g.p.hp;
            g.hurtPlayer(10.f, start + V2(-10, 0), A_WOLF);
            check(g.p.hp == hp0, "a rolling hunter cannot be hurt");
            for (int i = 0; i < 20; i++) g.updatePlayer(0.016f);
            check(g.p.pos.x > start.x + 20.f, "the roll carries the hunter along its direction");
            check(g.p.rollT <= 0 && g.p.rollCd > 0, "the roll ends and a cooldown starts");
            g.in.dodgePressed = true;
            g.updatePlayer(0.016f);
            g.in.dodgePressed = false;
            check(g.p.rollT <= 0, "no second roll during the cooldown");
            for (int i = 0; i < 80; i++) g.updatePlayer(0.016f);
            g.p.stamina = 5.f;
            g.in.dodgePressed = true;
            g.updatePlayer(0.016f);
            g.in.dodgePressed = false;
            check(g.p.rollT <= 0, "a winded hunter cannot roll");
            g.in = Input();
            g.p.sk = keepSk;
            g.p.invuln = 0.f;
        }
        // status effects: weapons stun, scare or poison animals; stun freezes, fear makes them bolt, poison kills over time; legendaries resist
        {
            const SkillSet keepSk = g.p.sk;
            g.newGame();
            g.p.dead = false;
            g.state = ST_PLAY;
            g.animals.clear();
            check(weaponEffect(W_HVYAXE).stunChance > 0 && weaponEffect(W_BOGSPEAR).poisonT > 0 && weaponEffect(W_SUNJAV).fearChance > 0 && weaponEffect(W_FLINT).stunChance == 0, "the effect table: axes stun, the bog spear poisons, the sun javelin scares, a knife does nothing");
            Animal* d = g.spawnAnimal(A_DEER, g.p.pos + V2(90, 0), 0, 0);
            check(d != nullptr, "(setup) a deer for the effects test");
            if (d) {
                d->stunT = 1.f;
                const V2 at = d->pos;
                for (int i = 0; i < 20; i++) g.updateAnimals(0.02f);
                check(dist(d->pos, at) < 1.f && d->stunT > 0.5f, "a stunned animal stays where it is");
                for (int i = 0; i < 60; i++) g.updateAnimals(0.02f);
                check(d->stunT <= 0, "the stun wears off");
                d->fearT = 2.f;
                d->st = AS_REST;
                g.updateAnimals(0.02f);
                check(d->st == AS_FLEE && !d->fleeAnimal, "a scared animal bolts from the hunter");
                d->hp = d->maxhp = 20;
                d->poisonT = 30.f;
                d->poisonTick = 0.5f;
                for (int i = 0; i < 1600 && !d->dead; i++) g.updateAnimals(0.02f);
                check(d->dead, "poison kills in the end");
            }
            g.animals.clear();
            Animal* l = g.spawnAnimal(A_DEER, g.p.pos + V2(90, 0), 0, TR_LEGENDARY);
            Animal* c = g.spawnAnimal(A_DEER, g.p.pos + V2(-90, 0), 0, 0);
            if (l && c) {
                g.applyHitEffects(*l, W_BOGSPEAR, false);
                g.applyHitEffects(*c, W_BOGSPEAR, false);
                check(c->poisonT > 0 && l->poisonT > 0 && l->poisonT < c->poisonT, "a legendary animal gets half the poison time");
                int stuns = 0;
                for (int i = 0; i < 400; i++) { c->stunT = 0; g.applyHitEffects(*c, W_HVYAXE, false); if (c->stunT > 0) stuns++; }
                check(stuns > 40 && stuns < 130, "a heavy axe stuns about one hit in five");
            }
            g.animals.clear();
            g.p.sk = keepSk;
        }
        // bosses: a lair in every wild biome; the boss wakes when the hunter comes close, attacks with telegraphed patterns, pays out and returns later
        {
            const SkillSet keepSk = g.p.sk;
            g.newGame();
            g.p.dead = false;
            g.state = ST_PLAY;
            bool allOk = true;
            for (int i = 0; i < BOSS_N; i++) {
                g.loadMapWorld(BOSSES[i].biome);
                g.animals.clear();
                const V2 lair = g.bossLairPos();
                allOk = allOk && !g.world.solidTile((int)(lair.x / TS), (int)(lair.y / TS)) && !g.world.water((int)(lair.x / TS), (int)(lair.y / TS));
            }
            check(allOk, "every boss lair stands on open dry ground");
            g.loadMapWorld(B_DESERT);
            g.bossLairMap = -1;
            g.animals.clear();
            g.p.pos = g.bossLairPos() + V2(BOSS_SEEN_RANGE + 80.f, 0);
            g.updateBosses(0.05f);
            check(g.liveBoss() == nullptr, "far from its lair the boss sleeps");
            check(!g.bossSeen[B_DESERT], "the lair is not marked on the map before the hunter has been near");
            g.p.pos = g.bossLairPos() + V2(BOSS_SEEN_RANGE - 20.f, 0);
            g.updateBosses(0.05f);
            check(g.bossSeen[B_DESERT] && !g.liveBoss(), "coming near finds the lair without waking the boss");
            g.p.pos = g.bossLairPos() + V2(BOSS_WAKE_RANGE - 40.f, 0);
            g.updateBosses(0.05f);
            Animal* b = g.liveBoss();
            check(b != nullptr && b->kind == A_DUNELION && b->tier == TR_LEGENDARY && b->hp > ANIMAL[A_DUNELION].hp * TIER[TR_LEGENDARY].hp * 2.f, "the desert boss wakes with extra health");
            if (b) {
                g.updateBosses(0.05f);
                check(g.animals.size() == 1, "only one boss at a time");
                // an attack: charge wind-up, rush, hurts an unprotected hunter; a roll's i-frames protect
                g.p.pos = b->pos + V2(120.f, 0);
                g.p.invuln = 0.f;
                b->bossCd = 0.f;
                b->busyT = 0.f;
                b->stunT = 0.f;
                g.updateBosses(0.05f);
                check(b->bossAct == 1 || b->bossAct == 3, "in reach it starts a telegraphed attack");
                const int act0 = b->bossAct;
                g.p.pos = b->pos + V2(act0 == 3 ? 20.f : 60.f, 0);  // the hunter stands in its way
                for (int i = 0; i < 120 && b->bossAct != 0 && g.p.hp == g.p.maxhp; i++) { g.updateBosses(0.016f); g.p.invuln = 0.f; }
                check(g.p.hp < g.p.maxhp, "an attack that lands hurts the hunter");
                g.p.hp = g.p.maxhp;
                // the pack is called at two thirds of its health
                b->bossAct = 0;
                b->hp = b->maxhp * 3 / 5;
                const size_t n0 = g.animals.size();
                g.updateBosses(0.05f);
                check(g.animals.size() == n0 + 2 && b->bossCalls == 1, "wounded, it calls its pack");
                // it never runs away
                b->st = AS_FLEE;
                b->fleeAnimal = false;
                g.updateBosses(0.05f);
                check(b->st == AS_CHASE, "a boss does not flee");
                // the kill pays out and starts the respawn clock
                const int coins0 = g.p.coins;
                g.killAnimal(*b, true);
                check(g.p.coins > coins0 && g.bossKills[B_DESERT] == 1 && g.bossCool[B_DESERT] > 0, "killing the boss pays coins and starts its respawn timer");
                g.p.pos = g.bossLairPos();
                g.animals.clear();
                g.updateBosses(0.05f);
                check(g.liveBoss() == nullptr, "a slain boss does not return at once");
                g.bossCool[B_DESERT] = 0.01f;
                g.updateBosses(0.05f);
                g.updateBosses(0.05f);
                check(g.liveBoss() != nullptr, "when the timer runs out the boss is back");
            }
            g.bossKills[B_SWAMP] = 3;
            g.bossCool[B_SWAMP] = 123.f;
            g.bossSeen[B_SWAMP] = true;
            g.save();
            g.newGame();
            check(g.bossKills[B_SWAMP] == 0, "(setup) a new game forgets the bosses");
            check(g.load() && g.bossKills[B_SWAMP] == 3 && g.bossCool[B_SWAMP] > 100.f && g.bossSeen[B_SWAMP], "boss kills, respawn timers and found lairs survive a save / load");
            g.loadMapWorld(B_HOME);
            g.bossLairMap = -1;
            g.animals.clear();
            g.p.sk = keepSk;
        }
        // melee: tap = quick swing, three swings in a row = combo finisher, hold = heavy swing; reach and arc matter; swinging costs stamina
        {
            const SkillSet keepSk = g.p.sk;
            g.newGame();
            g.p.dead = false;
            g.state = ST_PLAY;
            g.animals.clear();
            g.in = Input();
            check(meleeDef(W_AXE).dmgMul > meleeDef(W_FLINT).dmgMul && meleeDef(W_AXE).dur > meleeDef(W_FLINT).dur, "axes hit harder and slower than knives");
            check(meleeDef(W_SPEAR).reach > meleeDef(W_AXE).reach && meleeDef(W_SPEAR).arcDeg < meleeDef(W_AXE).arcDeg, "spears reach farther through a narrower arc");
            g.p.aim = V2(1, 0);
            auto tap = [&](float hold) {  // press, hold, release (as the input does), then let the swing play out
                g.in.melee = true;
                for (float t = 0; t < hold; t += 0.02f) g.updateMelee(0.02f);
                g.in.melee = false;
                g.updateMelee(0.02f);
                for (int i = 0; i < 80; i++) g.updateMelee(0.02f);
            };
            Animal* d = g.spawnAnimal(A_DEER, g.p.pos + V2(22, 0), 0, 0);
            Animal* far = g.spawnAnimal(A_DEER, g.p.pos + V2(160, 0), 0, 0);
            Animal* behind = g.spawnAnimal(A_DEER, g.p.pos + V2(-22, 0), 0, 0);
            check(d && far && behind, "(setup) animals in front, behind and far");
            if (d && far && behind) {
                d->hp = d->maxhp = behind->hp = behind->maxhp = far->hp = far->maxhp = 5000;
                g.p.stamina = 100.f;
                tap(0.05f);
                check(d->hp < 5000, "a quick swing hurts an animal in front of the hunter");
                check(far->hp == 5000 && behind->hp == 5000, "it misses animals that are too far or behind");
                check(g.p.stamina < 100.f, "swinging costs stamina");
                // combo: three quick swings inside the window; the third deals more
                g.p.meleeComboT = 0.f;
                std::vector<int> dealt;
                for (int i = 0; i < 3; i++) {
                    const int before = d->hp;
                    g.p.cd = 0.f;
                    g.p.stamina = 100.f;
                    g.in.melee = true;
                    g.updateMelee(0.02f);
                    g.in.melee = false;
                    g.updateMelee(0.02f);
                    for (int k = 0; k < 40 && g.p.meleeT > 0; k++) g.updateMelee(0.02f);
                    dealt.push_back(before - d->hp);
                    d->pos = g.p.pos + V2(22, 0);
                    d->kb = V2();
                    d->stunT = 0;
                }
                check(g.p.meleeStep == 2 && dealt[2] > dealt[0] * 1.2f, "the third swing of a combo is a stronger finisher");
                // heavy: held long enough
                g.p.meleeComboT = 0.f;
                g.p.cd = 0.f;
                g.p.stamina = 100.f;
                d->hp = 5000;
                d->pos = g.p.pos + V2(22, 0);
                const int hq = d->hp;
                g.in.melee = true;
                for (float t = 0; t < MELEE_HEAVY_HOLD + 0.05f; t += 0.02f) g.updateMelee(0.02f);
                g.in.melee = false;
                g.updateMelee(0.02f);
                check(g.p.meleeHeavy && g.p.meleeT > 0, "holding the button builds a heavy swing");
                for (int k = 0; k < 20 && !g.p.meleeStruck; k++) g.updateMelee(0.02f);
                check(hq - d->hp > dealt[0] * 1.8f && d->stunT > 0, "the heavy swing hits much harder and stuns");
                // too tired
                for (int k = 0; k < 60; k++) g.updateMelee(0.02f);
                g.p.cd = 0.f;
                g.p.stamina = 1.f;
                const int hp1 = d->hp;
                g.in.melee = true;
                g.updateMelee(0.02f);
                g.in.melee = false;
                g.updateMelee(0.02f);
                check(g.p.meleeT <= 0 && d->hp == hp1, "a winded hunter cannot swing");
            }
            g.animals.clear();
            g.in = Input();
            g.p.sk = keepSk;
            g.p.stamina = 100.f;
        }
        // tracking: animals leave fading prints; the Tracking skill widens and lengthens them; reading a fresh print pays XP
        {
            const SkillSet keepSk = g.p.sk;
            g.newGame();
            g.p.dead = false;
            g.state = ST_PLAY;
            g.animals.clear();
            g.tracks.clear();
            check(g.inWilds() && SK_TRACKING < SK_COUNT, "(setup) outdoors, and Tracking is a listed skill");
            Animal* d = g.spawnAnimal(A_DEER, g.p.pos + V2(60, 0), 0, 0);
            check(d != nullptr, "(setup) a deer stands near the hunter");
            if (d) {
                d->printAt = d->pos;
                for (int i = 0; i < 4; i++) { d->pos.x += TRACK_STEP + 1.f; g.updateTracks(0.05f); }
                check(g.tracks.size() == 4 && g.tracks[0].kind == A_DEER, "a walking animal leaves a print every few steps");
                const float life0 = g.tracks[0].life;
                const float t0 = g.tracks[0].t;
                g.updateTracks(1.f);
                check(g.tracks[0].t > t0, "prints age");
                g.p.sk.s[SK_TRACKING].xp = SkillState::capXp();
                g.tracks.clear();
                d->printAt = d->pos;
                d->pos.x += TRACK_STEP + 1.f;
                g.updateTracks(0.05f);
                check(!g.tracks.empty() && g.tracks[0].life > life0 * 1.5f, "a skilled tracker's prints last longer");
                g.tracks.clear();
                d->pos = g.p.pos + V2(500, 0);
                d->printAt = d->pos;
                d->pos.x += TRACK_STEP + 1.f;
                g.updateTracks(0.05f);
                check(g.tracks.empty(), "animals far from the hunter leave no prints");
                g.p.sk = keepSk;
                const int64_t xp0 = g.p.sk.s[SK_TRACKING].xp;
                g.tracks.push_back({g.p.pos, A_DEER, 0.f, TRACK_LIFE, false});
                g.trackReadCd = 0.f;
                g.updateTracks(0.05f);
                check(g.p.sk.s[SK_TRACKING].xp > xp0 && g.tracks.back().read, "standing on a fresh print reads it for Tracking XP");
                const int64_t xp1 = g.p.sk.s[SK_TRACKING].xp;
                g.trackReadCd = 0.f;
                g.updateTracks(0.05f);
                check(g.p.sk.s[SK_TRACKING].xp == xp1, "a print pays only once");
            }
            g.animals.clear();
            g.tracks.clear();
            g.p.sk = keepSk;
            g.p.dead = false;
        }
        // the mount: dismounting outdoors parks it there; walking away sends it home; the mount key summons it again
        {
            const SkillSet keepSk = g.p.sk;
            g.newGame();
            g.p.dead = false;
            g.state = ST_PLAY;
            g.hsAddAnimal(DA_MOUNT, g.p.pos, false, -1, A_WOLF, 1);
            HAnimal& m = g.hs.animals.back();
            m.age = 1e6f;
            m.saddled = true;
            check(g.inWilds(), "(setup) the hunter is outdoors");
            check(g.toggleMount() && g.riddenMount() == &m && !m.away, "outdoors the mount key calls the saddled mount and mounts it");
            g.p.pos += V2(100, 0);
            const V2 spot = g.p.pos;
            g.dismount();
            check(!g.riddenMount() && g.parked.active && m.away && dist(g.parked.pos, spot) < 8.f, "dismounting outdoors leaves the mount waiting where you got off");
            g.updateParked(0.5f);
            check(g.parked.active, "it stays while the hunter is within range");
            g.p.pos += V2(Game::PARK_RANGE - 20.f, 0);
            g.updateParked(0.5f);
            check(g.parked.active, "...still in range at the edge");
            g.p.pos += V2(60, 0);
            g.updateParked(0.5f);
            check(!g.parked.active && !m.away, "out of range it goes back to its pen");
            check(g.toggleMount() && g.riddenMount() == &m && dist(V2(m.px, m.py), g.p.pos) < 2.f, "the mount key summons it beside the hunter and mounts it");
            g.dismount();
            check(g.parked.active, "(setup) parked again");
            g.toggleMount();
            check(g.riddenMount() == &m && !g.parked.active && !m.away, "pressing the key next to the parked mount climbs on");
            g.dismount(true);
            check(!g.parked.active, "a quiet dismount (a building or cave) sends it home rather than parking it");
            m.saddled = false;
            m.companion = true;
            check(g.companionAnimal() == &m && !g.saddledMount(), "a companion is not a saddled mount (set in the animal panel)");
            {
                bool ok = true;
                for (const MountDef& md : MOUNTS) ok = ok && mountSpeed(md.kind, 0) > 1.1f && mountSpeed(md.kind, 0) < 1.5f && mountSpeed(md.kind, 3) < 1.65f;
                check(ok, "balance: every mount is clearly faster than walking but below the hunter's own sprint speed (x1.55)");
                check(MOUNT_SPRINT_MUL < 1.55f && MOUNT_SPRINT_DRAIN < 1.f, "balance: a mount's sprint adds less and tires the rider more slowly");
            }
            g.p.sk = keepSk;
            g.p.dead = false;
        }
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
        check(bc && animalClips(A_WOLF) && animalClips(A_DEER) && animalClips(A_RABBIT) && animalClips(A_HARE) && animalClips(A_SNOWHARE) && animalClips(A_FENNEC) && !animalClips(A_RATTLER) && g.A.rich[A_BOAR] && g.A.rich[A_RABBIT] && g.A.rich[A_FENNEC] && !g.A.rich[A_RATTLER], "every four-legged species, the hoppers and the birds have a full animation set; only the crawlers keep the walk cycle");
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
            {
                bool tiles = true;
                for (int sh = 0; sh < GRASS_SHADES; sh++) for (int v = 0; v < GRASS_VARIANTS; v++) tiles = tiles && g.A.grassT[sh][v].ok();
                for (int v = 0; v < SOIL_PATCHES; v++) tiles = tiles && g.A.soilPatch[v].ok();
                for (int i = 0; i < WATER_TEX_W; i++) tiles = tiles && g.A.waterW[i].ok();
                for (int i = 0; i < BANK_TEX_W / 16; i++) tiles = tiles && g.A.bankW[i].ok();
                for (int st = 0; st < LIP_COUNT; st++) for (int i = 0; i < 4; i++) tiles = tiles && g.A.lip[st][i].ok();
                for (int t = 0; t < 3; t++) { for (int v = 0; v < 4; v++) tiles = tiles && g.A.dirtTone[t][v].ok(); for (int v = 0; v < 2; v++) tiles = tiles && g.A.dirtRock[t][v].ok(); }
                check(tiles, "the grass, water, bank, raised-edge and dirt tile art is all built");
                check(tileRank(T_COBBLE) > tileRank(T_GRASS) && tileRank(T_GRASS) > tileRank(T_DIRT) && tileRank(T_DIRT) > tileRank(T_MUD) && tileRank(T_GRASS) == tileRank(T_DARK) && tileRank(T_WATER) < 0 && tileRank(T_FLOORW) < 0,
                      "raised ground: paving stands over grass, grass over dirt, dirt over mud; water and interiors have no raised edge");
                // the grass bends away from a hunter running past it and settles again
                size_t gi = g.world.decor.size();
                for (size_t i = 0; i < g.world.decor.size() && gi == g.world.decor.size(); i++)
                    if (g.world.decor[i].kind == D_TUFT) gi = i;
                if (gi < g.world.decor.size()) {
                    const Decor dc = g.world.decor[gi];
                    g.p.dead = false;
                    g.interior = -1;
                    g.p.pos = V2(dc.x - 6, dc.y);
                    g.updateGrass(1.f / 60.f);  // (sets the map's grass up)
                    g.p.pos.x += 2.f;           // 2 px in a sixtieth of a second: a run
                    g.updateGrass(1.f / 60.f);
                    const float bent = g.grassBend(gi);
                    g.timeAcc += Game::GRASS_REST + 0.1f;
                    check(bent > 5.f && g.grassBend(gi) == 0.f, "a tuft leans away from a hunter running past it (to the right of him: clockwise) and settles after a moment");
                    g.p.pos.x = dc.x - 6;
                    g.updateGrass(1.f);  // standing still next to it
                    g.p.pos.x = dc.x - 6;
                    g.updateGrass(1.f);
                    check(g.grassBend(gi) == 0.f, "standing still in the grass does not bend it");
                } else
                    check(false, "found a tuft of grass to test");
            }
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
                A_().stT = 10;
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
