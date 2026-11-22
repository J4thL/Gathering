// game_boss.cpp - boss animals: lairs, telegraphed attacks, the health bar, rewards and respawn (data in animals/bosses.hpp).
#include "core/game.hpp"
#include "core/logic_util.hpp"

// The lair: the walkable dry tile nearest the middle of the map (far from the gates, which sit at the edges).
V2 Game::bossLairPos() {
    if (bossLairMap == world.mapId) return bossLair;
    bossLairMap = world.mapId;
    const int cx = MW / 2, cy = MH / 2;
    bossLair = V2(cx * TS + TS / 2, cy * TS + TS / 2);
    for (int r = 0; r < std::max(MW, MH) / 2; r++) {
        bool found = false;
        for (int y = cy - r; y <= cy + r && !found; y++)
            for (int x = cx - r; x <= cx + r && !found; x++) {
                if (std::max(abs(x - cx), abs(y - cy)) != r) continue;
                if (world.solidTile(x, y) || world.water(x, y)) continue;
                bool open = true;  // a 3x3 patch of open ground, so the boss does not start wedged in
                for (int dy = -1; dy <= 1 && open; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        if (world.solidTile(x + dx, y + dy) || world.water(x + dx, y + dy)) { open = false; break; }
                if (!open) continue;
                bossLair = V2(x * TS + TS / 2, y * TS + TS / 2);
                found = true;
            }
        if (found) break;
    }
    return bossLair;
}

Animal* Game::liveBoss() {
    for (auto& a : animals)
        if (a.boss && !a.dead && !a.gone) return &a;
    return nullptr;
}

void Game::bossKilled(Animal& a) {
    const int b = a.boss - 1;
    const BossDef* bd = bossForBiome(b);
    if (!bd || b < 0 || b >= B_COUNT) return;
    bossKills[b]++;
    bossCool[b] = BOSS_RESPAWN;
    const int pay = bd->coins + 40 * std::min(bossKills[b] - 1, 5);
    p.coins += pay;
    grantXp(SK_HUNTING, 250 + 60 * std::min(bossKills[b] - 1, 5));
    shakeAmt = 7.f;
    audio.play(S_FELL, 0.8f);
    msg(std::string(bd->name) + " IS SLAIN!  +" + S(pay) + " COINS", C_GOLD);
}

namespace {
// is q inside the cone from o (reach, half-angle) around dir?
bool inCone(V2 o, V2 dir, V2 q, float reach, float half) {
    const V2 d = q - o;
    const float L = d.len();
    if (L > reach) return false;
    if (L < 6.f) return true;
    const float c = (d.x * dir.x + d.y * dir.y) / L;
    return c >= cosf(half);
}
// distance from q to the segment o -> o + dir * len
float segDist(V2 o, V2 dir, float len, V2 q) {
    const V2 d = q - o;
    const float t = std::max(0.f, std::min(len, d.x * dir.x + d.y * dir.y));
    return dist(q, o + dir * t);
}
float phaseTime(const BossMoveDef& m, int ph, bool enr) {
    float t = ph == BP_PREP ? m.prep : ph == BP_WARN ? m.warn : ph == BP_EXEC ? m.exec : m.rec;
    if (enr && ph == BP_WARN) t = std::max(std::min(t, BOSS_MIN_WARN), t * BOSS_ENRAGE_SPEED);
    else if (enr && (ph == BP_PREP || ph == BP_RECOVER)) t *= BOSS_ENRAGE_SPEED;
    return t;
}
}  // namespace

// One hit on the hunter from the boss (a dodge-roll's invulnerability makes it miss).
void Game::bossStrike(Animal& a, float mul, bool combo) {
    const int hp0 = p.hp;
    hurtPlayer(ANIMAL[a.kind].dmg * mul * (a.bossEnr ? BOSS_ENRAGE_DMG : 1.f), a.pos, a.kind);
    if (combo && p.hp < hp0 && !p.dead) p.invuln = std::min(p.invuln, 0.25f);  // a flurry: the next blow can follow quickly
}

// pick the next attack from the boss's moveset that fits the distance to the hunter
void Game::bossChoose(Animal& a, float dp) {
    const BossDef* bd = bossForBiome(a.boss - 1);
    if (!bd) return;
    int ok[12], n = 0;
    for (int i = 0; i < 12 && bd->moves[i] != BM_COUNT; i++) {
        const int m = bd->moves[i];
        if (m == BM_SUMMON || m == BM_ENRAGE) continue;
        if (dp < BMOVE[m].minD || dp > BMOVE[m].maxD) continue;
        if (m == a.bossLast && n > 0) continue;  // not the same thing twice
        ok[n++] = m;
    }
    if (n == 0) return;
    // up close the melee attacks are likelier, far off the ranged ones
    const int m = ok[rng.next() % n];
    a.bossMove = m;
    bossEnter(a, BP_PREP);
}

// starts a phase of the current attack
void Game::bossEnter(Animal& a, int phase) {
    const BossMoveDef& M = BMOVE[a.bossMove];
    a.bossAct = phase;
    a.bossT = phaseTime(M, phase, a.bossEnr);
    a.busyT = a.bossT + 0.05f;
    a.bossHit = false;
    a.bossStep = 0;
    a.bossAux = 0;
    const V2 toP = (p.pos - a.pos).norm();
    if (phase == BP_PREP) {
        a.bossDir = toP;
        a.face = toP.x >= 0 ? 1 : -1;
        audio.play(S_ERROR, 0.4f);
    } else if (phase == BP_WARN) {
        // the aim is locked now: a sideways step or a roll gets out of the way
        a.bossDir = toP;
        a.bossTgt = p.pos;
        a.face = toP.x >= 0 ? 1 : -1;
        if (a.bossMove == BM_JUMP) {
            const float d = std::min(dist(a.pos, p.pos), M.maxD);
            a.bossTgt = a.pos + toP * d;
        } else if (a.bossMove == BM_METEOR) {
            bossPts.clear();
            const int n = BOSS_METEORS + (a.bossEnr ? 1 : 0);
            for (int i = 0; i < n; i++) {
                const float ang = frng.range(0.f, 6.283f), r = i == 0 ? 0.f : frng.range(20.f, 85.f);
                bossPts.push_back(p.pos + V2(cosf(ang), sinf(ang)) * r);
            }
        } else if (a.bossMove == BM_CATACLYSM) {
            const float ang = frng.range(0.f, 6.283f), r = frng.range(60.f, M.size - 40.f);
            bossSafe = a.pos + V2(cosf(ang), sinf(ang)) * r;
            msg("STAND IN THE SAFE ZONE!", C_GOLD);
        }
        audio.play(S_ERROR, 0.5f);
    } else if (phase == BP_EXEC) {
        switch (a.bossMove) {
            case BM_BASIC: case BM_HEAVY: case BM_SWEEP:
                if (inCone(a.pos, a.bossDir, p.pos, M.size, M.size2)) {
                    bossStrike(a, M.dmg);
                    if (a.bossMove == BM_HEAVY) { pRootT = std::max(pRootT, 0.4f); }
                }
                if (a.bossMove != BM_BASIC) shakeAmt = std::max(shakeAmt, 4.f);
                audio.play(S_HIT, 0.7f);
                break;
            case BM_SLAM:
                if (dist(a.pos, p.pos) < M.size) { bossStrike(a, M.dmg); pSlowT = std::max(pSlowT, 1.5f); }
                shakeAmt = std::max(shakeAmt, 5.f);
                audio.play(S_HIT, 0.8f);
                puff(a.pos, 10, HEX(0xcdb98f));
                break;
            case BM_GRAB:
                if (dist(a.pos, p.pos) < M.size) {  // caught: held in the boss's grip
                    bossStrike(a, M.dmg);
                    pRootT = std::max(pRootT, 1.2f);
                    p.pos = a.pos + toP * 18.f;
                    p.kb = V2();
                    msg("GRABBED!", C_RED);
                }
                audio.play(S_HIT, 0.7f);
                break;
            case BM_PROJECTILE: {
                const int n = a.bossEnr ? 3 : 1;
                for (int i = 0; i < n; i++) {
                    const float off = n == 1 ? 0.f : (i - 1) * 0.3f;
                    const float c = cosf(off), s = sinf(off);
                    const V2 d(a.bossDir.x * c - a.bossDir.y * s, a.bossDir.x * s + a.bossDir.y * c);
                    bshots.push_back({a.pos + d * 12.f, d, M.size / BOSS_SHOT_SPEED + 0.2f, ANIMAL[a.kind].dmg * M.dmg * (a.bossEnr ? BOSS_ENRAGE_DMG : 1.f)});
                }
                audio.play(S_HIT, 0.5f);
                break;
            }
            case BM_EXPLOSION:
                if (dist(a.pos, p.pos) < M.size) { bossStrike(a, M.dmg); pSlowT = std::max(pSlowT, 1.f); }
                shakeAmt = std::max(shakeAmt, 6.f);
                audio.play(S_HIT, 0.9f);
                puff(a.pos, 16, HEX(0xe08a3c));
                break;
            case BM_SUMMON: {
                const BossDef* bd = bossForBiome(a.boss - 1);
                const int n = a.bossEnr ? 3 : 2;
                for (int i = 0; i < n && bd; i++) {
                    const float ang = frng.range(0.f, 6.283f);
                    Animal* m = spawnAnimal(bd->packKind, a.pos + V2(cosf(ang), sinf(ang)) * 36.f, 0, 0);
                    if (m) { m->st = AS_CHASE; m->stT = 10.f; }
                }
                a.bossCalls++;
                msg(std::string(bd ? bd->name : "THE BOSS") + " CALLS ITS PACK!", C_RED);
                audio.play(S_FELL, 0.5f);
                break;
            }
            case BM_ROAR:
                if (dist(a.pos, p.pos) < M.size) {
                    bossStrike(a, M.dmg);
                    pWeakT = std::max(pWeakT, 6.f);
                    pRootT = std::max(pRootT, 0.7f);
                    ftext(p.pos + V2(0, -34), "WEAKENED", C_RED, 1.4f);
                }
                shakeAmt = std::max(shakeAmt, 5.f);
                audio.play(S_FELL, 0.7f);
                break;
            case BM_ENRAGE:
                a.bossEnr = true;
                shakeAmt = std::max(shakeAmt, 6.f);
                audio.play(S_FELL, 0.8f);
                msg("ENRAGED!  IT STRIKES FASTER AND HARDER", C_RED);
                puff(a.pos, 12, HEX(0xd8443f));
                break;
            case BM_BREATH: case BM_SPIN:
                a.bossAux = 0;  // ticks run in bossRun
                break;
            case BM_MULTI: break;
            case BM_METEOR: break;
            case BM_CATACLYSM:
                if (dist(a.pos, p.pos) < M.size && dist(bossSafe, p.pos) > M.size2) bossStrike(a, M.dmg);
                shakeAmt = std::max(shakeAmt, 7.f);
                audio.play(S_HIT, 1.f);
                puff(a.pos, 20, HEX(0xe08a3c));
                break;
            default: break;
        }
    }
}

// the attack that is playing: the countdown of its phase and what each moment does
void Game::bossRun(Animal& a, float dt) {
    const BossMoveDef& M = BMOVE[a.bossMove];
    const AnimalDef& D = ANIMAL[a.kind];
    a.bossT -= dt;
    a.busyT = std::max(a.busyT, 0.05f);
    const float el = phaseTime(M, a.bossAct, a.bossEnr) - a.bossT;  // seconds into the phase
    if (a.bossAct == BP_EXEC) {
        switch (a.bossMove) {
            case BM_CHARGE: {  // the rush along the red line
                a.face = a.bossDir.x >= 0 ? 1 : -1;
                stepAnimal(a, a.bossDir, M.size / M.exec, dt);
                if (!a.bossHit && dist(a.pos, p.pos) < D.hitR * a.scale + 12.f) {
                    a.bossHit = true;
                    bossStrike(a, M.dmg);
                    pSlowT = std::max(pSlowT, 1.f);
                }
                break;
            }
            case BM_JUMP: {  // the leap to where the hunter stood
                const V2 to = a.bossTgt;
                const float rem = std::max(a.bossT, 0.03f);
                const V2 d = (to - a.pos);
                if (d.len() > 2.f) stepAnimal(a, d.norm(), d.len() / rem, dt);
                break;
            }
            case BM_MULTI: {  // three blows in a row, each aimed anew
                const float step = M.exec / 3.f;
                if (el >= step * a.bossStep && a.bossStep < 3) {
                    a.bossStep++;
                    a.bossDir = (p.pos - a.pos).norm();
                    a.face = a.bossDir.x >= 0 ? 1 : -1;
                    if (inCone(a.pos, a.bossDir, p.pos, M.size, M.size2)) bossStrike(a, M.dmg, true);
                    audio.play(S_HIT, 0.6f);
                    puff(a.pos + a.bossDir * 24.f, 3, HEX(0xdddddd));
                }
                break;
            }
            case BM_SPIN: {  // turning on the spot, hitting whoever is near
                a.bossAux -= dt;
                const V2 d = (p.pos - a.pos).norm();
                stepAnimal(a, d, D.run * 0.5f, dt);
                a.face = ((int)(el * 12.f) & 1) ? 1 : -1;
                if (a.bossAux <= 0) {
                    a.bossAux = 0.4f;
                    if (dist(a.pos, p.pos) < M.size) bossStrike(a, M.dmg, true);
                    audio.play(S_HIT, 0.4f);
                }
                break;
            }
            case BM_BREATH: {  // a stream down the cone; the ticks keep hurting while the hunter stays in it
                a.bossAux -= dt;
                if (a.bossAux <= 0) {
                    a.bossAux = 0.3f;
                    if (inCone(a.pos, a.bossDir, p.pos, M.size, M.size2)) {
                        bossStrike(a, M.dmg, true);
                        if (a.kind == A_POLARBEAR) pSlowT = std::max(pSlowT, 1.5f);
                    }
                    puff(a.pos + a.bossDir * (30.f + frng.range(0.f, M.size - 30.f)), 2, HEX(0xdff3ff));
                }
                break;
            }
            case BM_METEOR: {  // the circles land one after the other
                const int n = (int)bossPts.size();
                while (a.bossStep < n && el >= M.exec * a.bossStep / n) {
                    const V2 pt = bossPts[a.bossStep];
                    if (dist(pt, p.pos) < M.size) bossStrike(a, M.dmg);
                    puff(pt, 8, HEX(0xe08a3c));
                    shakeAmt = std::max(shakeAmt, 3.f);
                    audio.play(S_HIT, 0.5f);
                    a.bossStep++;
                }
                break;
            }
            default: break;
        }
    }
    if (a.bossT > 0) return;
    // the phase is over
    if (a.bossAct == BP_PREP) bossEnter(a, BP_WARN);
    else if (a.bossAct == BP_WARN) bossEnter(a, BP_EXEC);
    else if (a.bossAct == BP_EXEC) {
        if (a.bossMove == BM_JUMP) {  // the landing
            if (dist(a.pos, p.pos) < M.size) { bossStrike(a, M.dmg); pRootT = std::max(pRootT, 0.3f); }
            shakeAmt = std::max(shakeAmt, 6.f);
            audio.play(S_HIT, 0.9f);
            puff(a.pos, 12, HEX(0xcdb98f));
        }
        bossEnter(a, BP_RECOVER);
    } else {
        a.bossAct = BP_IDLE;
        a.bossLast = a.bossMove;
        a.bossCd = a.bossEnr ? BOSS_COOLDOWN * 0.6f : BOSS_COOLDOWN;
        a.busyT = 0.f;
        bossPts.clear();
    }
}

void Game::updateBossShots(float dt) {
    for (size_t i = 0; i < bshots.size();) {
        BossShot& s = bshots[i];
        s.pos += s.dir * (BOSS_SHOT_SPEED * dt);
        s.life -= dt;
        bool gone = s.life <= 0 || world.solidTile((int)(s.pos.x / TS), (int)(s.pos.y / TS));
        if (!gone && dist(s.pos, p.pos) < 9.f) {
            hurtPlayer(s.dmg, s.pos - s.dir * 10.f, A_GATOR);
            gone = true;
        }
        if (gone) { puff(s.pos, 3, HEX(0xc8e060)); bshots.erase(bshots.begin() + i); }
        else i++;
    }
}

void Game::updateBosses(float dt) {
    for (int k = 0; k < B_COUNT; k++)
        if (bossCool[k] > 0) bossCool[k] = std::max(0.f, bossCool[k] - dt);
    pWeakT = std::max(0.f, pWeakT - dt);
    pRootT = std::max(0.f, pRootT - dt);
    if (delve < 0) pSlowT = std::max(0.f, pSlowT - dt);  // (inside a delve its own update counts it down)
    if (!inWilds() || p.dead) { bshots.clear(); bossPts.clear(); return; }
    updateBossShots(dt);
    const int b = world.mapId;
    const BossDef* bd = bossForBiome(b);
    if (!bd) return;
    const V2 lair = bossLairPos();
    const float dl = dist(p.pos, lair);
    if (dl < BOSS_SEEN_RANGE) bossSeen[b] = true;
    Animal* boss = liveBoss();
    if (!boss && bossCool[b] <= 0 && dl < BOSS_WAKE_RANGE) {  // the hunter has walked into the lair
        Animal* a = spawnAnimal(bd->kind, lair, 0, TR_LEGENDARY);
        if (a) {
            a->boss = b + 1;
            a->maxhp = a->hp = std::max(1, (int)lroundf(a->maxhp * bd->hpMul));
            a->bossCd = 2.f;
            a->st = AS_CHASE;
            a->stT = 10.f;
            a->hpShow = 4.f;
            msg(std::string(bd->name) + " ROUSES FROM ITS LAIR!", C_RED);
            audio.play(S_FELL, 0.6f);
            shakeAmt = 4.f;
        }
        boss = liveBoss();
    }
    if (!boss) { bossPts.clear(); return; }
    Animal& a = *boss;
    const float dp = dist(a.pos, p.pos);
    a.hpShow = std::max(a.hpShow, dp < 360.f ? 1.f : 0.f);
    // a boss never breaks off a fight (unless it is terrified)
    if (a.st == AS_FLEE && !a.fleeAnimal && a.fearT <= 0 && dp < 400.f) { a.st = AS_CHASE; a.stT = 10.f; }
    a.calmT = 0;
    if (a.bossAct != BP_IDLE) {  // an attack is playing
        if (a.stunT > 0 && a.bossAct != BP_EXEC) a.stunT = 0;  // a boss shrugs off being stunned in the middle of an attack
        bossRun(a, dt);
        return;
    }
    a.bossCd -= dt;
    if (a.stunT > 0 || a.busyT > 0 || a.bossCd > 0) return;
    // half its health gone: it enrages (once); at two thirds and one third it calls its pack. Both are warned about like any attack.
    if (!a.bossEnr && a.hp * 2 < a.maxhp) { a.bossMove = BM_ENRAGE; bossEnter(a, BP_PREP); return; }
    if ((a.bossCalls == 0 && a.hp * 3 < a.maxhp * 2) || (a.bossCalls == 1 && a.hp * 3 < a.maxhp)) { a.bossMove = BM_SUMMON; bossEnter(a, BP_PREP); return; }
    if (dp < BOSS_ACT_RANGE) bossChoose(a, dp);
}

// ---- the warnings on the ground. Shapes are drawn as flat ground marks (circles squashed to 0.8 like everything on the ground).
namespace {
// fills every pixel inside(dx, dy) (world offsets from the centre) with `fill`, and the edge of each row with `rim`
template <class F>
void groundShape(Game& g, int cx, int cy, int rx, int ry, F inside, u32 fill, u32 rim) {
    const int y0 = std::max(cy - ry, 0), y1 = std::min(cy + ry, LH - 1);
    for (int y = y0; y <= y1; y++) {
        int runStart = -1;
        const int xa = std::max(cx - rx, 0), xb = std::min(cx + rx, LW - 1);
        for (int x = xa; x <= xb + 1; x++) {
            const bool in = x <= xb && inside((float)(x - cx), (float)(y - cy) / 0.8f);
            if (in && runStart < 0) runStart = x;
            if (!in && runStart >= 0) {
                g.fillRect(runStart, y, x - runStart, 1, fill);
                const bool cap = y == y0 || y == y1;
                if (cap) g.fillRect(runStart, y, x - runStart, 1, rim);
                else { g.fillRect(runStart, y, 2, 1, rim); g.fillRect(x - 2, y, 2, 1, rim); }
                runStart = -1;
            }
        }
    }
}
}  // namespace

void Game::drawBossTelegraph() {
    for (auto& s : bshots) {  // shots in flight
        const int sx = (int)s.pos.x - camX, sy = (int)s.pos.y - camY;
        fillRect(sx - 3, sy - 3, 7, 7, RGBA(120, 170, 40, 200));
        fillRect(sx - 2, sy - 2, 5, 5, RGBA(214, 240, 90, 255));
    }
    for (auto& a : animals) {
        if (!a.boss || a.dead || a.gone || a.bossAct == BP_IDLE) continue;
        const BossMoveDef& M = BMOVE[a.bossMove];
        const int ax = (int)a.pos.x - camX, ay = (int)a.pos.y - camY;
        const float pulse = 0.75f + 0.25f * sinf(timeAcc * 22.f);
        if (a.bossAct == BP_PREP) {  // 1: the boss readies itself
            textC(ax, ay - 46, "!", C_RED, 2, true);
            continue;
        }
        if (a.bossAct == BP_RECOVER) continue;  // 5: nothing to avoid; the boss is open
        const bool exec = a.bossAct == BP_EXEC;
        const float prog = exec ? 1.f : 1.f - a.bossT / std::max(0.05f, phaseTime(M, BP_WARN, a.bossEnr));  // how close the warning is to going off
        const int fa = exec ? 150 : (int)((35 + 70 * prog) * pulse), ra = exec ? 255 : (int)(190 * pulse);
        const u32 fill = RGBA(226, 52, 38, fa), rim = RGBA(255, 96, 70, ra);
        const V2 dir = a.bossDir;
        auto circle = [&](int cxp, int cyp, float R, u32 f, u32 r) {
            groundShape(*this, cxp, cyp, (int)R + 1, (int)(R * 0.8f) + 1, [R](float dx, float dy) { return dx * dx + dy * dy <= R * R; }, f, r);
        };
        switch (M.shape) {
            case BS_CIRCLE:  // slam, grab, spin: the area around the boss
                circle(ax, ay, M.size, fill, rim);
                break;
            case BS_CONE: {  // a frontal attack or the breath
                const float reach = M.size, half = M.size2;
                const float cs = cosf(half);
                groundShape(*this, ax, ay, (int)reach + 1, (int)(reach * 0.8f) + 1, [&](float dx, float dy) {
                    const float L = sqrtf(dx * dx + dy * dy);
                    return L <= reach && (L < 4.f || (dx * dir.x + dy * dir.y) / L >= cs);
                }, fill, rim);
                break;
            }
            case BS_LINE: {  // a charge or a shot's path
                const int n = (a.bossMove == BM_PROJECTILE && a.bossEnr) ? 3 : 1;
                for (int k = 0; k < n; k++) {
                    const float off = n == 1 ? 0.f : (k - 1) * 0.3f;
                    const V2 d(dir.x * cosf(off) - dir.y * sinf(off), dir.x * sinf(off) + dir.y * cosf(off));
                    const float len = M.size, w = M.size2;
                    const int R = (int)(len + w) + 1;
                    groundShape(*this, ax, ay, R, (int)(R * 0.8f) + 1, [&](float dx, float dy) {
                        const float t = dx * d.x + dy * d.y;
                        if (t < 0 || t > len) return false;
                        return fabsf(dx * d.y - dy * d.x) <= w;
                    }, fill, rim);
                }
                break;
            }
            case BS_CIRCLES:  // meteors: a circle for each one still to land
                for (size_t i = (size_t)a.bossStep; i < bossPts.size(); i++)
                    circle((int)bossPts[i].x - camX, (int)bossPts[i].y - camY, M.size, fill, rim);
                break;
            case BS_EXPAND: {  // an expanding circle: it will be dangerous out to the rim
                circle(ax, ay, M.size, RGBA(226, 52, 38, 22), rim);
                const float r = exec ? M.size : std::max(4.f, M.size * prog);
                circle(ax, ay, r, fill, RGBA(255, 150, 110, ra));
                break;
            }
            case BS_CROSSHAIR: {  // the spot it will land on
                const int tx = (int)a.bossTgt.x - camX, ty = (int)a.bossTgt.y - camY;
                circle(tx, ty, M.size, fill, rim);
                const u32 c = RGBA(255, 70, 50, 230);
                fillRect(tx - 9, ty, 18, 1, c);
                fillRect(tx, ty - 7, 1, 14, c);
                fillRect(tx - 12, ty, 4, 1, c);
                fillRect(tx + 9, ty, 4, 1, c);
                break;
            }
            case BS_SAFE: {  // red everywhere except the green zone
                circle(ax, ay, M.size, fill, rim);
                const int sx = (int)bossSafe.x - camX, sy = (int)bossSafe.y - camY;
                circle(sx, sy, M.size2, RGBA(70, 220, 110, 120), RGBA(150, 255, 170, 255));
                textC(sx, sy - 8, "SAFE", HEX(0xcffff0), 1, true);
                break;
            }
            default: break;
        }
        if (!exec) textC(ax, ay - 40, M.name, C_RED, 1, true);
    }
}

// the boss's name and health across the top while it fights
void Game::drawWildBossBar() {
    Animal* b = liveBoss();
    if (!b || dist(b->pos, p.pos) > 360.f) return;
    const BossDef* bd = bossForBiome(b->biome);
    const char* nm = bd ? bd->name : "BOSS";
    const int w = 150, x = (LW - w) / 2, y = 14;
    textC(LW / 2, y - 9, nm, C_RED, 1, true);
    bar(x, y, w, 4, (float)b->hp / b->maxhp, HEX(0xd8443f), HEX(0x2a1210));
}
