// monsters.cpp - the creatures of the delves: movement round the walls, telegraphed attacks, damage, drops, XP, and the Frostbound King.
//
// A monster wakes when the hunter comes within sight, then follows a flow field (steps to the hunter, round walls) until it is close enough
// to attack. Every attack has a windup (the animation and, for the big ones, a warning circle on the floor) so it can be dodged.
#include <algorithm>
#include "core/game.hpp"

namespace {
const float PI = 3.14159265f;
struct AtkInfo { float windup, total; };
// boss attacks: 0 swing, 1 slam, 2 ice bolts, 3 charge, 4 summon, 5 blizzard, 6 nova
const AtkInfo BOSS_ATK[7] = {{0.70f, 1.15f}, {0.90f, 1.45f}, {0.80f, 1.35f}, {0.60f, 1.35f}, {1.00f, 1.70f}, {0.80f, 4.30f}, {1.10f, 1.80f}};
}  // namespace

static void monsterHurt(Game& g, float dmg, V2 from, bool ice) {
    if (ice && g.p.has(CG_FROSTWARD)) dmg *= 0.70f;  // the frostward cloak turns a third of the cold
    if (g.p.has(CG_CROWN)) dmg *= 0.90f;
    g.hurtPlayer(dmg, from, -1);
}

void Game::playerPoison(float secs) {
    if (p.has(CG_VENOMWRAPS)) secs *= 0.5f;
    pPoisonT = std::max(pPoisonT, secs);
}

Monster* Game::spawnMonster(int kind, V2 pos, float depth, bool elite, int id) {
    const MonsterDef& D = MONDEF(kind);
    Monster m;
    m.kind = kind;
    m.id = id;
    m.pos = m.home = pos;
    m.depth = depth;
    m.elite = elite;
    const bool boss = (D.flags & MF_BOSS) != 0;
    const float hpK = boss ? 1.f : (1.f + 1.2f * depth) * (elite ? 1.8f : 1.f);
    const float dmK = boss ? 1.f : (1.f + 0.8f * depth) * (elite ? 1.25f : 1.f);
    m.maxhp = m.hp = std::max(1, (int)(D.hp * hpK));
    m.dmg = D.dmg * dmK;
    m.spd = D.spd * (elite ? 1.1f : 1.f);
    m.face8 = rng.irange(0, F_COUNT - 1);
    m.packId = rng.irange(0, 3);
    m.special = 2.5f;
    mons.push_back(m);
    return &mons.back();
}

void Game::addHazard(int kind, V2 pos, float r, float delay, float dmg, bool slow) {
    Hazard h;
    h.kind = kind;
    h.pos = pos;
    h.r = r;
    h.delay = delay;
    h.dmg = dmg;
    h.slow = slow;
    hazards.push_back(h);
}

void Game::shootBolt(V2 from, V2 dir, float dmg, float speed) {
    MProj b;
    b.pos = from;
    b.dir = dir.norm();
    b.dmg = dmg;
    b.speed = speed;
    mprojs.push_back(b);
}

// ------------------------------------------------------------------ the flow field
void Game::buildFlow() {
    const int tx = (int)floorf(p.pos.x / TS), ty = (int)floorf(p.pos.y / TS);
    flow.assign((size_t)MW * MH, -1);
    flowTx = tx;
    flowTy = ty;
    if (tx < 0 || ty < 0 || tx >= MW || ty >= MH) return;
    std::vector<int> q;
    q.reserve(4096);
    q.push_back(ty * MW + tx);
    flow[ty * MW + tx] = 0;
    for (size_t h = 0; h < q.size(); h++) {
        const int c = q[h], cx = c % MW, cy = c / MW, d = flow[c];
        if (d >= 70) continue;
        static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
            const int nx = cx + DX[k], ny = cy + DY[k];
            if (nx < 1 || ny < 1 || nx >= MW - 1 || ny >= MH - 1) continue;
            if (flow[ny * MW + nx] >= 0 || world.solidTile(nx, ny)) continue;
            flow[ny * MW + nx] = (int16_t)(d + 1);
            q.push_back(ny * MW + nx);
        }
    }
}

// slide along walls like the animals do
bool Game::monsterStep(Monster& m, V2 dir, float speed, float dt) {
    static const float offs[7] = {0, 0.6f, -0.6f, 1.2f, -1.2f, 1.9f, -1.9f};
    const float base = atan2f(dir.y, dir.x);
    const float r = MONDEF(m.kind).hitR * 0.6f + 1.5f;
    for (int k = 0; k < 7; k++) {
        const float ang = base + offs[k] * m.bias;
        const V2 d(cosf(ang), sinf(ang));
        const V2 np = m.pos + d * (speed * dt);
        if (!world.blocked(np.x, np.y, r)) {
            if (k > 0 && (k % 2 == 0)) m.bias = -m.bias;
            m.pos = np;
            m.vel = d * speed;
            return true;
        }
    }
    m.vel = V2();
    return false;
}

// the way towards the hunter: straight when it can be seen, otherwise down the flow field
static V2 chaseDir(Game& g, const Monster& m, bool& have) {
    have = true;
    const V2 to = g.p.pos - m.pos;
    if (to.len() < 40.f || g.lineClear(m.pos, g.p.pos, -1, -1)) return to.norm();
    const int tx = (int)floorf(m.pos.x / TS), ty = (int)floorf(m.pos.y / TS);
    if (tx < 1 || ty < 1 || tx >= MW - 1 || ty >= MH - 1 || g.flow.empty() || g.flow[ty * MW + tx] < 0) {
        have = false;
        return V2();
    }
    int best = g.flow[ty * MW + tx], bx = tx, by = ty;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy) continue;
            const int nx = tx + dx, ny = ty + dy, v = g.flow[ny * MW + nx];
            if (v < 0 || v >= best) continue;
            if (dx && dy && (g.world.solidTile(tx + dx, ty) || g.world.solidTile(tx, ty + dy))) continue;  // no cutting corners
            best = v, bx = nx, by = ny;
        }
    if (bx == tx && by == ty) return to.norm();
    return (V2(bx * TS + 8.f, by * TS + 8.f) - m.pos).norm();
}

// ------------------------------------------------------------------ combat
bool Game::hitMonster(Monster& m, float dmg, V2 dir, bool crit, int wpn) {
    if (m.state == MN_DEAD) return true;
    const MonsterDef& D = MONDEF(m.kind);
    const bool boss = (D.flags & MF_BOSS) != 0;
    m.awake = true;
    if ((D.flags & MF_BLOCK) && m.state != MN_ATK && m.state != MN_HURT && m.state != MN_CAST) {  // a raised shield
        float fx, fy;
        facingVec(m.face8, fx, fy);
        if (fx * -dir.x + fy * -dir.y > 0.35f && rng.chance(boss ? 0.25f : 0.5f)) {
            ftext(m.pos + V2(0, -D.bodyH - 12), "BLOCKED", C_DIM, 0.8f);
            audio.play(S_CLICK, 0.7f);
            puff(m.pos + V2(0, -D.bodyH), 4, HEX(0xdfeaf4));
            m.flash = 0.08f;
            return false;
        }
    }
    if ((D.flags & MF_UNDEAD) && p.has(CG_SILVERCHARM)) dmg *= 1.12f;
    if ((D.flags & MF_UNDEAD) && wpn == W_SILVERDAGGER) dmg *= 1.35f;
    const int d = std::max(1, (int)roundf(dmg));
    m.hp -= d;
    m.flash = 0.16f;
    m.hpShow = 4.f;
    const bool heavy = (D.flags & MF_SLAM) || boss;
    if (!boss) m.kb += dir * ((crit ? 120.f : 80.f) * (heavy ? 0.3f : 1.f));
    if (!boss && !((D.flags & MF_SLAM) && m.state == MN_ATK) && m.state != MN_DEAD) {  // a big attack in progress is not interrupted
        m.state = MN_HURT;
        m.stT = 0;
        m.hurtT = heavy ? 0.2f : 0.3f;
    }
    if (D.flags & MF_ICE) {
        for (int i = 0; i < (crit ? 9 : 5); i++) {
            const float a = rng.range(0, 6.28f), sp = rng.range(30, 90);
            parts.push_back({m.pos.x, m.pos.y, rng.range(4, D.bodyH + 4), cosf(a) * sp, sinf(a) * sp * 0.6f, rng.range(30, 90), 0.6f, 0.6f, rng.chance(0.5f) ? HEX(0xcdf1ff) : HEX(0x8ecff0), 1, true});
        }
    } else if (m.kind == MK_GOLEM) {
        for (int i = 0; i < 6; i++) parts.push_back({m.pos.x, m.pos.y, rng.range(4, 14), rng.range(-70, 70), rng.range(-30, 30), rng.range(30, 90), 0.5f, 0.5f, HEX(0x8a7a68), 1, true});
    } else bloodBurst(m.pos + V2(0, -D.bodyH), dir, crit ? 12 : 7);
    ftext(m.pos + V2(0, -D.bodyH - 12), (crit ? std::string("CRIT ") : std::string()) + S(d), crit ? C_ORANGE : C_TEXT, 0.9f);
    audio.play(crit ? S_CRIT : S_HIT);
    shakeAmt = std::max(shakeAmt, crit ? 2.5f : 1.2f);
    if (m.hp <= 0) killMonster(m);
    return true;
}

void Game::dropLoot(const Monster& m) {
    const MonsterDef& D = MONDEF(m.kind);
    const bool boss = (D.flags & MF_BOSS) != 0;
    auto drop = [&](int mat, int n) {
        if (n <= 0) return;
        GroundItem g{};
        g.pos = m.pos + V2(rng.range(-12, 12), rng.range(-4, 8));
        g.item = itemMat(mat);
        g.n = n;
        g.ammo = -1;
        g.lock = 0.5f;
        items.push_back(g);
    };
    for (const MonDrop& d : D.drops) {
        if (d.chance <= 0) continue;
        float c = d.chance * (m.elite ? 1.5f : 1.f);
        if (boss || rng.chance(std::min(1.f, c))) drop(d.mat, rng.irange(d.lo, d.hi));
    }
    if (m.elite && !boss) drop(M_ICESHARD + (rng.chance(0.5f) ? 0 : 0), 1);
    if (m.elite && delve == DV_FROST2) drop(M_CRYPTKEY, 1);  // the crypt's elites carry the keys to the locked treasure rooms
    const int coins = (int)(D.coins * (1.f + m.depth) * (m.elite ? 2.f : 1.f) * rng.range(0.7f, 1.3f));
    if (coins > 0) {
        p.coins += coins;
        ftext(m.pos + V2(0, -28), "+" + S(coins) + " COINS", C_GOLD, 1.0f);
        audio.play(S_COIN, 0.5f);
    }
}

void Game::killMonster(Monster& m) {
    const MonsterDef& D = MONDEF(m.kind);
    const bool boss = (D.flags & MF_BOSS) != 0;
    m.state = MN_DEAD;
    m.stT = 0;
    m.deadT = 0;
    m.hp = 0;
    kills++;
    delveKills++;
    audio.play(S_KILL);
    shakeAmt = std::max(shakeAmt, boss ? 9.f : 2.f);
    const float xpK = (1.f + 0.6f * m.depth) * (m.elite ? 1.8f : 1.f);
    const int xp = (int)(D.xp * xpK);
    grantXp(SK_HUNTING, xp);
    grantXp(SK_RANGED, std::max(1, xp / 2));
    if (boss) {
        grantXp(SK_DEFENSE, 400);
        DelveMem& dm = delveMem[delve];
        dm.bossDead = true;
        dm.bossTimer = 1800.f;
        bossIdx = -1;
        hazards.clear();
        mprojs.clear();
        for (Monster& o : mons)  // his summons crumble with him
            if (o.state != MN_DEAD && o.kind != MK_KING) { o.hp = 0; o.state = MN_DEAD; o.deadT = 0; }
        DChest c;
        c.pos = V2(world.bossPos.x, world.bossPos.y - 60.f);
        for (const DSpawn& s : world.dsp)
            if (s.type == DS_CHEST && s.id == DELVE_BOSS_CHEST) c.pos = V2(s.x, s.y);
        c.tier = 3;
        c.id = DELVE_BOSS_CHEST;
        c.locked = false;
        if (!(dm.chests & (1u << DELVE_BOSS_CHEST))) dchests.push_back(c);
        msg("THE FROSTBOUND KING HAS FALLEN!", C_GOLD);
        msg("HIS TREASURE CHEST APPEARS BEFORE THE THRONE", C_GOLD);
    }
    dropLoot(m);
}

// ------------------------------------------------------------------ attacks
static void startAttack(Game& g, Monster& m, int type, int state) {
    m.state = state;
    m.stT = 0;
    m.struck = false;
    m.atkType = type;
    m.aim = (g.p.pos - m.pos).norm();
    m.face8 = facingFromDir(m.aim.x, m.aim.y, m.face8);
}

void Game::monsterAttack(Monster& m, float dt) {
    const MonsterDef& D = MONDEF(m.kind);
    const V2 toP = p.pos - m.pos;
    const float d = toP.len();
    const bool ice = (D.flags & MF_ICE) != 0;
    if (m.state == MN_CAST || (m.state == MN_ATK && m.atkType == 0 && (D.flags & MF_RANGED))) {
        // the mage: tracks the hunter while it gathers the spell, fires at the end of the windup
        if (m.stT < D.windup) {
            m.aim = toP.norm();
            m.face8 = facingFromDir(m.aim.x, m.aim.y, m.face8);
        }
        if (!m.struck && m.stT >= D.windup) {
            m.struck = true;
            shootBolt(m.pos + V2(0, -D.bodyH), m.aim, m.dmg, 120.f);
            if (m.elite) {
                shootBolt(m.pos + V2(0, -D.bodyH), V2(m.aim.x * cosf(0.3f) - m.aim.y * sinf(0.3f), m.aim.x * sinf(0.3f) + m.aim.y * cosf(0.3f)), m.dmg * 0.7f, 120.f);
                shootBolt(m.pos + V2(0, -D.bodyH), V2(m.aim.x * cosf(-0.3f) - m.aim.y * sinf(-0.3f), m.aim.x * sinf(-0.3f) + m.aim.y * cosf(-0.3f)), m.dmg * 0.7f, 120.f);
            }
            audio.play(S_THROW, 0.5f);
        }
        if (m.stT >= D.windup + 0.35f + D.recover * 0.5f) { m.state = MN_CHASE; m.atkCd = D.cd; m.stT = 0; }
        return;
    }
    if (m.state == MN_CAST) {  // golem slam: the circle was placed at the start, nothing more to do but wait
        if (m.stT >= D.windup + 0.4f) { m.state = MN_CHASE; m.atkCd = D.cd; m.stT = 0; }
        return;
    }
    // melee
    const float strikeT = D.windup;
    if ((D.flags & (MF_LUNGE)) || m.kind == MK_BAT) {
        if (m.stT >= strikeT && m.stT < strikeT + 0.28f) monsterStep(m, m.aim, (m.kind == MK_BAT ? 170.f : 210.f), dt);
    }
    if (!m.struck && m.stT >= strikeT) {
        const bool reach = d < D.range + D.hitR * 0.5f + 5.f;
        float fx = m.aim.x, fy = m.aim.y;
        const float facing = reach && d > 0.1f ? (fx * toP.x + fy * toP.y) / d : -1.f;
        if (reach && (facing > 0.15f || d < 14.f)) {
            m.struck = true;
            monsterHurt(*this, m.dmg, m.pos, ice);
            if (D.flags & MF_POISON) playerPoison(5.f);
            if (ice && (m.kind == MK_FROSTWOLF || m.kind == MK_ICEGOLEM)) pSlowT = std::max(pSlowT, 1.6f);
            audio.play(S_HIT, 0.6f);
        } else if (m.stT >= strikeT + 0.28f)
            m.struck = true;  // whiffed
    }
    if (m.stT >= strikeT + 0.3f + D.recover) {
        m.state = MN_CHASE;
        m.atkCd = D.cd * rng.range(0.85f, 1.2f);
        m.stT = 0;
    }
}

void Game::bossThink(Monster& m, float dt) {
    const MonsterDef& D = MONDEF(m.kind);
    const V2 toP = p.pos - m.pos;
    const float d = toP.len();
    const int ph = m.hp * 3 > m.maxhp * 2 ? 1 : (m.hp * 3 > m.maxhp ? 2 : 3);
    if (ph != m.phase) {  // a new phase: he roars, the room shakes and the dead rise
        m.phase = ph;
        shakeAmt = 8.f;
        msg(ph == 2 ? "THE KING'S EYES BLAZE!" : "THE KING IS ENRAGED!", C_BLUE);
        ftext(m.pos + V2(0, -50), ph == 2 ? "PHASE 2" : "PHASE 3", C_BLUE, 1.6f);
        startAttack(*this, m, 4, MN_CAST);
        m.atkCd = 0.f;
    }
    const AtkInfo& A = BOSS_ATK[std::max(0, std::min(6, m.atkType))];
    if (m.state == MN_ATK || m.state == MN_CAST) {
        const float t = m.stT;
        const int T = m.atkType;
        if (T == 0 || T == 1 || T == 2 || T == 6) {
            if (t < A.windup * 0.7f) {
                m.aim = toP.norm();
                m.face8 = facingFromDir(m.aim.x, m.aim.y, m.face8);
            }
        }
        if (T == 3 && t >= A.windup && t < A.windup + 0.55f) monsterStep(m, m.aim, 240.f, dt);  // the charge
        if (!m.struck && t >= A.windup) {
            m.struck = true;
            const float k = ph == 3 ? 1.2f : 1.f;
            switch (T) {
                case 0:
                    if (d < D.range + 14.f) { monsterHurt(*this, m.dmg * k, m.pos, true); pSlowT = std::max(pSlowT, 1.5f); }
                    puff(m.pos + m.aim * 24, 8, HEX(0xdff6ff));
                    audio.play(S_HIT, 0.8f);
                    break;
                case 1: audio.play(S_FELL, 0.8f); shakeAmt = 6.f; break;  // (the circle was placed at the start of the windup)
                case 2: {
                    const int n = ph == 1 ? 3 : (ph == 2 ? 5 : 7);
                    for (int i = 0; i < n; i++) {
                        const float a = atan2f(m.aim.y, m.aim.x) + (i - (n - 1) / 2.f) * 0.22f;
                        shootBolt(m.pos + V2(0, -22), V2(cosf(a), sinf(a)), m.dmg * 0.6f, 125.f);
                    }
                    audio.play(S_THROW, 0.7f);
                    break;
                }
                case 3: break;
                case 4: {
                    int alive = 0;
                    for (const Monster& o : mons)
                        if (o.kind != MK_KING && o.state != MN_DEAD) alive++;
                    const int want = std::max(0, std::min(2 + ph - 1, 6 - alive));
                    for (int i = 0; i < want; i++) {
                        const float a = rng.range(0, 6.28f);
                        const V2 sp = m.pos + V2(cosf(a), sinf(a)) * 46.f;
                        if (world.blocked(sp.x, sp.y, 6.f)) continue;
                        Monster* s = spawnMonster(i % 2 ? MK_ICESKEL : MK_FROSTWOLF, sp, 0.6f, false, -1);
                        s->awake = true;
                        s->state = MN_CHASE;
                        puff(sp, 8, HEX(0xdff6ff));
                    }
                    audio.play(S_FELL, 0.6f);
                    break;
                }
                case 5:  // the blizzard: ice rains round the hunter for a few seconds (placed over time below)
                    break;
                case 6: audio.play(S_FELL, 0.9f); shakeAmt = 7.f; break;
            }
        }
        if (T == 5 && t >= A.windup && t < A.total - 0.4f) {
            m.special -= dt;
            if (m.special <= 0.f) {  // a falling shard every 0.35 s near the hunter
                m.special = 0.35f;
                const V2 at = p.pos + V2(rng.range(-26, 26), rng.range(-20, 20));
                addHazard(HZ_BLIZZ, at, 16.f, 0.7f, m.dmg * 0.5f, true);
            }
        }
        if (t >= A.total) {
            m.state = MN_CHASE;
            m.stT = 0;
            m.atkCd = ph == 1 ? 1.5f : (ph == 2 ? 1.1f : 0.75f);
            m.special = 0.f;
        }
        return;
    }
    // choose the next attack
    if (m.atkCd > 0.f) return;
    int pick;
    const float r = rng.f();
    if (d < 38.f) pick = r < 0.6f ? 0 : 1;
    else if (d < 110.f) pick = r < 0.35f ? 3 : (r < 0.7f ? 2 : (ph >= 2 && r < 0.85f ? 6 : 1));
    else pick = r < 0.4f ? 2 : (r < 0.7f ? 3 : (ph >= 2 ? 6 : 2));
    if (ph >= 2 && rng.chance(0.18f)) pick = 4;
    if (ph == 3 && rng.chance(0.25f)) pick = 5;
    startAttack(*this, m, pick, (pick == 0 || pick == 3) ? MN_ATK : MN_CAST);
    const float k = ph == 3 ? 1.2f : 1.f;
    if (pick == 1) addHazard(HZ_SLAM, m.pos + m.aim * 6, 58.f, BOSS_ATK[1].windup + 0.1f, m.dmg * 1.1f * k, true);
    if (pick == 6) addHazard(HZ_NOVA, m.pos, 98.f, BOSS_ATK[6].windup + 0.2f, m.dmg * 1.2f * k, true);
    if (pick == 3) m.aim = (p.pos - m.pos).norm();
}

void Game::updateMonsters(float dt) {
    if (flowT <= 0 || (int)floorf(p.pos.x / TS) != flowTx || (int)floorf(p.pos.y / TS) != flowTy) {
        buildFlow();
        flowT = 0.35f;
    }
    flowT -= dt;
    bossIdx = -1;
    for (size_t i = 0; i < mons.size();) {
        Monster& m = mons[i];
        const MonsterDef& D = MONDEF(m.kind);
        const bool boss = (D.flags & MF_BOSS) != 0;
        m.animT += dt;
        m.stT += dt;
        if (m.flash > 0) m.flash -= dt;
        if (m.hpShow > 0) m.hpShow -= dt;
        if (m.atkCd > 0) m.atkCd -= dt;
        if (m.slowT > 0) m.slowT -= dt;
        if (m.state == MN_DEAD) {
            m.deadT += dt;
            if (m.deadT > 30.f) { mons.erase(mons.begin() + i); continue; }
            i++;
            continue;
        }
        // knock-back
        if (m.kb.len() > 4.f) {
            const V2 np = m.pos + m.kb * dt;
            if (!world.blocked(np.x, np.y, 4.f)) m.pos = np;
            m.kb = m.kb * powf(0.02f, dt);
        } else m.kb = V2();
        const V2 toP = p.pos - m.pos;
        const float d = toP.len();
        if (!m.awake) {
            const float sight = D.aggro * (p.nightT > 0 ? 1.f : 1.f);
            if (!p.dead && d < sight && (d < sight * 0.45f || lineClear(m.pos, p.pos, -1, -1))) {
                m.awake = true;
                if (boss) { bossBanT = 4.f; audio.play(S_FELL, 0.8f); msg("THE FROSTBOUND KING RISES!", C_BLUE); }
                if (m.state == MN_IDLE) m.state = MN_CHASE;
                // a pack wakes together
                if (D.flags & MF_PACK)
                    for (Monster& o : mons)
                        if (o.kind == m.kind && !o.awake && o.state == MN_IDLE && dist(o.pos, m.pos) < 140.f) { o.awake = true; o.state = MN_CHASE; }
            } else {
                if (m.state == MN_IDLE && m.kind == MK_BAT) m.animT += dt;
                i++;
                continue;
            }
        } else if (d > D.aggro * 2.4f && !boss) {
            m.awake = false;
            m.state = MN_IDLE;
            i++;
            continue;
        }
        if (boss) bossIdx = (int)i;
        const float spd = m.spd * (m.slowT > 0 ? 0.6f : 1.f);
        if (m.state == MN_HURT) {
            if (m.stT >= m.hurtT) { m.state = MN_CHASE; m.stT = 0; }
        } else if (m.state == MN_IDLE) {
            m.state = MN_CHASE;
        } else if (m.state == MN_CHASE) {
            bool have;
            V2 dir = chaseDir(*this, m, have);
            const bool ranged = (D.flags & MF_RANGED) != 0;
            const bool los = d < D.range + 6 || lineClear(m.pos, p.pos, -1, -1);
            if (!p.dead) {
                float stop = ranged ? D.range * 0.7f : D.range * 0.8f;
                if (boss) stop = 26.f;
                if (ranged && d < D.range * 0.45f && los) {  // keeps its distance
                    monsterStep(m, V2() - toP.norm(), spd * 0.8f, dt);
                } else if (have && d > stop) {
                    // wolves and bats circle a little instead of running straight in
                    if ((D.flags & MF_LUNGE) && d < 70.f) {
                        const V2 side(-dir.y, dir.x);
                        dir = (dir + side * (0.7f * (m.packId & 1 ? 1.f : -1.f))).norm();
                    }
                    if (m.kind == MK_BAT) dir = (dir + V2(-dir.y, dir.x) * 0.5f * sinf(m.animT * 5.f + m.id)).norm();
                    monsterStep(m, dir, spd, dt);
                }
                if (d > 0.5f) m.face8 = facingFromDir(toP.x, toP.y, m.face8);
                if (boss) {
                    bossThink(m, dt);
                } else if (m.atkCd <= 0.f && d < D.range + D.hitR * 0.5f + (D.flags & MF_LUNGE ? 22.f : 0.f) + (ranged ? 0.f : 3.f) && (!ranged || los)) {
                    startAttack(*this, m, 0, ranged || (D.flags & MF_SLAM) ? MN_CAST : MN_ATK);
                    if (D.flags & MF_SLAM) {
                        m.aim = toP.norm();
                        addHazard(HZ_SLAM, m.pos + m.aim * 22.f, 28.f, D.windup, m.dmg, (D.flags & MF_ICE) != 0);
                    }
                    if (ranged && m.elite) addHazard(HZ_BURST, p.pos, 20.f, 1.0f, m.dmg * 0.8f, true);
                }
            }
        } else {
            if (boss) bossThink(m, dt);
            else monsterAttack(m, dt);
        }
        // bats flutter above the floor; keep them off the walls
        i++;
    }
}

// ------------------------------------------------------------------ hazards, bolts and traps
void Game::updateHazards(float dt) {
    for (size_t i = 0; i < hazards.size();) {
        Hazard& h = hazards[i];
        h.t += dt;
        if (!h.fired && h.t >= h.delay) {
            h.fired = true;
            const float d = dist(p.pos, h.pos);
            if (!p.dead && d < h.r + 3.f) {
                monsterHurt(*this, h.dmg, h.pos, h.slow);
                if (h.slow) pSlowT = std::max(pSlowT, 2.f);
            }
            const u32 col = h.kind == HZ_ROCK ? HEX(0x8a7a68) : HEX(0xdff6ff);
            const int n = h.kind == HZ_NOVA ? 26 : (int)(h.r / 3);
            for (int k = 0; k < n; k++) {
                const float a = rng.range(0, 6.28f), rr = rng.range(0, h.r);
                parts.push_back({h.pos.x + cosf(a) * rr, h.pos.y + sinf(a) * rr * 0.7f, rng.range(0, 6), cosf(a) * 30, sinf(a) * 20, rng.range(30, 90), 0.6f, 0.6f, col, 1 + (k & 1), true});
            }
            shakeAmt = std::max(shakeAmt, h.kind == HZ_ROCK ? 3.f : 4.f);
            audio.play(h.kind == HZ_ROCK ? S_BREAK : S_FELL, 0.5f);
            if (h.kind == HZ_ROCK) decals.push_back({h.pos, 0, 0, 30});
        }
        if (h.fired && h.t >= h.delay + h.after) { hazards.erase(hazards.begin() + i); continue; }
        i++;
    }
}

void Game::updateMProjs(float dt) {
    for (size_t i = 0; i < mprojs.size();) {
        MProj& b = mprojs[i];
        b.t += dt;
        bool done = b.t > b.life;
        const float step = b.speed * dt;
        const int sub = std::max(1, (int)ceilf(step / 3.f));
        for (int k = 0; k < sub && !done; k++) {
            b.pos += b.dir * (step / sub);
            if (!p.dead && dist(b.pos, p.pos + V2(0, -10)) < 8.f) {
                monsterHurt(*this, b.dmg, b.pos - b.dir * 6.f, true);
                pSlowT = std::max(pSlowT, 1.2f);
                done = true;
            } else if (world.solidTile((int)floorf(b.pos.x / TS), (int)floorf((b.pos.y + 9.f) / TS))) done = true;
        }
        if (rng.chance(dt * 30.f)) parts.push_back({b.pos.x, b.pos.y + 9.f, 6, rng.range(-8, 8), rng.range(-8, 8), rng.range(0, 10), 0.3f, 0.3f, HEX(0xaee8ff), 1, false});
        if (done) {
            puff(b.pos + V2(0, 8), 5, HEX(0xdff6ff));
            mprojs.erase(mprojs.begin() + i);
        } else i++;
    }
}

void Game::updateTraps(float dt) {
    for (DTrap& t : dtraps) {
        const float prev = t.t;
        t.t = fmodf(t.t + dt, TRAP_CYCLE);
        if (p.dead) continue;
        const float d = dist(p.pos, t.pos);
        if (t.kind == TR_SPIKE) {
            if (prev < 2.2f && t.t >= 2.2f) audio.play(S_CLICK, 0.4f);
            if (t.t >= 2.2f && t.t < 2.9f && d < 9.f) monsterHurt(*this, 15.f * (1.f + 0.8f * t.depth), t.pos, false);
        } else {
            if (t.t >= 2.2f && t.t < 3.0f) {
                if (rng.chance(dt * 40.f)) parts.push_back({t.pos.x + rng.range(-4, 4), t.pos.y, 2, rng.range(-6, 6), rng.range(-4, 4), rng.range(40, 80), 0.6f, 0.6f, HEX(0xdff6ff), 2, false});
                if (d < 14.f && pSlowT < 1.f) {
                    monsterHurt(*this, 9.f * (1.f + 0.8f * t.depth), t.pos, true);
                    pSlowT = 2.2f;
                }
            }
        }
    }
}
