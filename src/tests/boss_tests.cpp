// boss_tests.cpp - `wildwood --boss`: every boss attack played through the real game code, headless.
// Each attack must follow prepare -> warning -> execute -> recover, never hurt before the execute step, hurt a hunter who stays in the danger area
// and miss one who steps out of it. With BOSS_SHOTS=<dir> set, a screenshot of every warning shape is saved there.
#include <cstdio>
#include <cstdlib>
#include <string>
#include "core/game.hpp"

namespace {
struct Run { bool warned = false, early = false, exec = false, finished = false; int lost = 0, warnTicks = 0; };

Animal* wake(Game& g, int biome) {
    g.loadMapWorld(biome);
    g.bossLairMap = -1;
    g.animals.clear();
    g.bossPts.clear();
    g.bshots.clear();
    g.bossCool[biome] = 0;
    g.p.pos = g.bossLairPos() + V2(BOSS_WAKE_RANGE - 40.f, 0);
    g.updateBosses(0.05f);
    Animal* b = g.liveBoss();
    if (b) { b->bossCd = 99.f; b->bossAct = BP_IDLE; b->busyT = 0; b->stunT = 0; }
    g.p.dead = false;
    g.p.hp = g.p.maxhp = 100000;
    g.pWeakT = g.pRootT = g.pSlowT = 0;
    return b;
}

float standOff(int m) {
    switch (m) {
        case BM_CHARGE: case BM_JUMP: return 100.f;
        case BM_PROJECTILE: return 120.f;
        case BM_BREATH: return 80.f;
        case BM_EXPLOSION: return 50.f;
        case BM_ROAR: return 60.f;
        case BM_GRAB: return 20.f;
        case BM_HEAVY: return 35.f;
        case BM_SWEEP: return 40.f;
        default: return 30.f;
    }
}

// mode 0: the hunter stays where he is; 1: steps out of the way when the warning appears; 2: (cataclysm) stands in the safe zone; 3: stands in the red
Run play(Game& g, Animal* b, int move, int mode, const std::string& shotPath = "") {
    Run r;
    g.p.dead = false;
    g.p.hp = g.p.maxhp = 100000;
    b->bossAct = BP_IDLE;
    b->bossEnr = false;
    b->pos = g.bossLairPos();
    g.p.pos = b->pos + V2(standOff(move), 0);
    g.p.kb = V2();
    b->bossMove = move;
    g.bossEnter(*b, BP_PREP);
    bool shot = false;
    for (int i = 0; i < 900 && b->bossAct != BP_IDLE; i++) {
        const int hp0 = g.p.hp;
        g.p.invuln = 0.f;
        g.updateBosses(0.016f);
        if (b->bossAct == BP_WARN) {
            if (!r.warned) {
                r.warned = true;
                if (mode == 1) g.p.pos = b->pos + V2(0, 200.f);
                if (mode == 2) g.p.pos = g.bossSafe;
                if (mode == 3) g.p.pos = b->pos;
            }
            r.warnTicks++;
            if (!shotPath.empty() && !shot && b->bossT < 0.3f * BMOVE[move].warn) {
                shot = true;
                g.camX = (int)b->pos.x - LW / 2;
                g.camY = (int)b->pos.y - LH / 2;
                g.p.invuln = 5.f;
                g.render();
                g.render();
                g.saveCanvas(shotPath.c_str());
            }
        }
        if (b->bossAct == BP_EXEC) r.exec = true;
        if (g.p.hp < hp0) {
            r.lost += hp0 - g.p.hp;
            if (b->bossAct == BP_PREP || b->bossAct == BP_WARN) r.early = true;
        }
    }
    r.finished = b->bossAct == BP_IDLE;
    return r;
}
}  // namespace

int bossTest(Game& g) {
    int bad = 0;
    auto check = [&](bool ok, const std::string& what) {
        printf("  [%s] %s\n", ok ? "ok" : "FAIL", what.c_str());
        if (!ok) bad++;
    };
    const char* shotDir = getenv("BOSS_SHOTS");
    g.newGame();
    g.state = ST_PLAY;

    printf("--- stage 1: the movesets\n");
    bool used[BM_COUNT] = {};
    bool valid = true;
    for (int i = 0; i < BOSS_N; i++) {
        int n = 0;
        for (int k = 0; k < 12 && BOSSES[i].moves[k] != BM_COUNT; k++, n++) {
            const int m = BOSSES[i].moves[k];
            valid = valid && m >= 0 && m < BM_COUNT;
            if (m >= 0 && m < BM_COUNT) used[m] = true;
        }
        check(n >= 6 && n < 12, std::string(BOSSES[i].name) + " has its own set of attacks");
    }
    check(valid, "every move in a moveset exists");
    bool all = true;
    for (int m = 0; m < BM_COUNT; m++) if (m != BM_SUMMON && m != BM_ENRAGE && !used[m]) all = false;
    check(all, "every attack pattern is used by some boss (the summon and the enrage come by themselves)");
    bool sane = true;
    for (int m = 0; m < BM_COUNT; m++) sane = sane && BMOVE[m].prep > 0 && BMOVE[m].warn >= BOSS_MIN_WARN && BMOVE[m].exec > 0 && BMOVE[m].rec > 0 && BMOVE[m].shape != BS_NONE;
    check(sane, "every attack has a prepare, a warning (long enough to react to), an execute and a recovery");

    printf("--- stage 2: each attack, stood in and stepped out of\n");
    Animal* b = wake(g, B_DESERT);
    check(b != nullptr, "the desert boss wakes");
    if (!b) return bad + 1;
    for (int m = 0; m < BM_COUNT; m++) {
        if (m == BM_SUMMON || m == BM_ENRAGE) continue;
        std::string shot;
        if (shotDir) shot = std::string(shotDir) + "/boss_" + BMOVE[m].name + ".bmp";
        for (auto& c : shot) if (c == ' ' || c == '!') c = '_';
        const bool safeKind = m == BM_CATACLYSM;
        const Run stay = play(g, b, m, safeKind ? 3 : 0, shot);
        const std::string nm = BMOVE[m].name;
        check(stay.warned && stay.exec && stay.finished, nm + ": prepare, warning, execute and recovery all happen");
        check(!stay.early, nm + ": nothing hurts before the execute step");
        check(stay.lost > 0, nm + ": a hunter who stays in the danger area is hurt");
        const Run away = play(g, b, m, safeKind ? 2 : 1);
        check(away.lost == 0, nm + ": " + (safeKind ? "standing in the safe zone" : "stepping out of the way") + " avoids it");
    }

    printf("--- stage 3: the telegraph, the timing and the effects\n");
    check(BMOVE[BM_CHARGE].shape == BS_LINE && BMOVE[BM_BREATH].shape == BS_CONE && BMOVE[BM_SLAM].shape == BS_CIRCLE && BMOVE[BM_METEOR].shape == BS_CIRCLES &&
              BMOVE[BM_EXPLOSION].shape == BS_EXPAND && BMOVE[BM_JUMP].shape == BS_CROSSHAIR && BMOVE[BM_CATACLYSM].shape == BS_SAFE,
          "the shapes: slam circle, breath cone, charge line, meteor circles, explosion expanding circle, leap crosshair, cataclysm safe zone");
    {   // the roar weakens and holds; a weakened hunter takes more
        const Run r = play(g, b, BM_ROAR, 0);
        (void)r;
        check(g.pWeakT > 0 && g.pRootT >= 0, "the roar weakens a hunter who is near");
        g.p.invuln = 0; g.pWeakT = 0;
        int h0 = g.p.hp; g.hurtPlayer(40.f, b->pos, A_COYOTE); const int normal = h0 - g.p.hp;
        g.p.invuln = 0; g.pWeakT = 5.f;
        h0 = g.p.hp; g.hurtPlayer(40.f, b->pos, A_COYOTE); const int weak = h0 - g.p.hp;
        check(weak > normal, "a weakened hunter takes more damage");
        g.pWeakT = 0; g.pRootT = 0;
    }
    {   // the grab holds the hunter
        play(g, b, BM_GRAB, 0);
        // (the hold has run out by now: check it right after the execute)
        b->bossAct = BP_IDLE; b->pos = g.bossLairPos(); g.p.pos = b->pos + V2(20.f, 0); g.p.invuln = 0;
        b->bossMove = BM_GRAB; g.bossEnter(*b, BP_EXEC);
        check(g.pRootT > 0.5f, "a grab holds the hunter in place");
        const V2 held = g.p.pos;
        g.update(0.1f);
        b = g.liveBoss();
        check(dist(g.p.pos, held) < 3.f || g.pRootT <= 0, "a held hunter cannot walk away");
        g.pRootT = 0;
        if (!b) return bad + 1;
        b->bossAct = BP_IDLE;
    }
    {   // summoning calls more creatures
        const size_t n0 = g.animals.size();
        const int c0 = b->bossCalls;
        b->bossAct = BP_IDLE;
        play(g, b, BM_SUMMON, 0);
        check(g.animals.size() > n0 && b->bossCalls == c0 + 1, "the summon brings more creatures and counts as a call");
    }
    {   // enrage: it comes by itself at half health, warned about, then everything is quicker and harder
        b->bossAct = BP_IDLE;
        b->bossEnr = false;
        b->bossCalls = 2;
        b->hp = b->maxhp * 2 / 5;
        b->bossCd = 0.f;
        g.p.pos = b->pos + V2(300.f, 0);
        bool saw = false;
        for (int i = 0; i < 300 && !b->bossEnr; i++) {
            g.updateBosses(0.016f);
            if (b->bossMove == BM_ENRAGE && b->bossAct == BP_WARN) saw = true;
        }
        check(saw && b->bossEnr, "below half health the boss enrages (after a warning)");
        b->bossAct = BP_IDLE;
        b->bossEnr = false;
        const Run calm = play(g, b, BM_HEAVY, 0);
        g.p.hp = g.p.maxhp = 100000;
        b->bossEnr = true;
        b->bossAct = BP_IDLE;
        b->pos = g.bossLairPos();
        g.p.pos = b->pos + V2(standOff(BM_HEAVY), 0);
        b->bossMove = BM_HEAVY;
        g.bossEnter(*b, BP_PREP);
        int warn2 = 0, lost2 = 0;
        for (int i = 0; i < 900 && b->bossAct != BP_IDLE; i++) {
            const int h0 = g.p.hp;
            g.p.invuln = 0;
            g.updateBosses(0.016f);
            if (b->bossAct == BP_WARN) warn2++;
            lost2 += h0 - g.p.hp;
        }
        check(warn2 < calm.warnTicks && warn2 * 0.016f >= BOSS_MIN_WARN - 0.02f, "an enraged boss winds up faster, but the warning is never too short to react to");
        check(lost2 > calm.lost, "an enraged boss hits harder");
        b->bossEnr = false;
    }
    {   // a boss slain in the middle of an attack leaves nothing behind
        b->bossAct = BP_IDLE;
        b->bossMove = BM_METEOR;
        g.p.dead = false;
        g.p.hp = g.p.maxhp = 100000;
        g.p.pos = b->pos + V2(40.f, 0);
        g.bossEnter(*b, BP_PREP);
        for (int i = 0; i < 100; i++) g.updateBosses(0.016f);
        check(!g.bossPts.empty(), "(the meteor shower has its circles)");
        g.killAnimal(*b, true);
        g.updateBosses(0.016f);
        check(g.liveBoss() == nullptr && g.bossPts.empty() && g.bossCool[B_DESERT] > 0, "a boss killed mid-attack leaves no warnings and starts its respawn timer");
    }

    printf("--- stage 4: every boss fights\n");
    for (int biome : {B_DESERT, B_SWAMP, B_SNOW, B_MOUNTAIN}) {
        Animal* bo = wake(g, biome);
        if (!bo) printf("    dbg wilds=%d dead=%d map=%d cool=%.1f lair=%.0f,%.0f\n", g.inWilds(), g.p.dead, g.world.mapId, g.bossCool[biome], g.bossLairPos().x, g.bossLairPos().y);
        check(bo != nullptr, std::string("the boss of biome ") + std::to_string(biome) + " wakes");
        if (!bo) continue;
        const BossDef* bd = bossForBiome(biome);
        bool each = true;
        for (int k = 0; k < 12 && bd->moves[k] != BM_COUNT; k++) {
            const Run r = play(g, bo, bd->moves[k], bd->moves[k] == BM_CATACLYSM ? 3 : 0);
            each = each && r.warned && r.exec && r.finished && !r.early;
        }
        check(each, std::string(bd->name) + " plays out its whole moveset in order");
        // left alone it picks attacks by itself
        bo->bossAct = BP_IDLE; bo->bossCd = 0.f; bo->busyT = 0;
        g.p.pos = bo->pos + V2(50.f, 0);
        int started = 0;
        for (int i = 0; i < 3000; i++) {
            g.p.invuln = 3.f;
            g.updateBosses(0.016f);
            if (bo->bossAct == BP_PREP) started++;
            g.p.pos = bo->pos + V2(50.f, 0);
        }
        check(started >= 3, std::string(bd->name) + " keeps choosing its own attacks");
    }
    printf("boss test: %d failure(s)\n", bad);
    return bad;
}
