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

void Game::updateBosses(float dt) {
    for (int k = 0; k < B_COUNT; k++)
        if (bossCool[k] > 0) bossCool[k] = std::max(0.f, bossCool[k] - dt);
    if (!inWilds() || p.dead) return;
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
    if (!boss) return;
    Animal& a = *boss;
    const AnimalDef& D = ANIMAL[a.kind];
    const float dp = dist(a.pos, p.pos);
    a.hpShow = std::max(a.hpShow, dp < 360.f ? 1.f : 0.f);
    // a boss never breaks off a fight (unless it is terrified)
    if (a.st == AS_FLEE && !a.fleeAnimal && a.fearT <= 0 && dp < 400.f) { a.st = AS_CHASE; a.stT = 10.f; }
    a.calmT = 0;
    const bool enraged = a.hp * 2 < a.maxhp;

    // the pack answers at two thirds and one third of its health
    if (a.bossAct == 0 && ((a.bossCalls == 0 && a.hp * 3 < a.maxhp * 2) || (a.bossCalls == 1 && a.hp * 3 < a.maxhp))) {
        a.bossCalls++;
        for (int i = 0; i < 2; i++) {
            const float ang = frng.range(0.f, 6.283f);
            Animal* m = spawnAnimal(bd->packKind, a.pos + V2(cosf(ang), sinf(ang)) * 36.f, 0, 0);
            if (m) { m->st = AS_CHASE; m->stT = 10.f; }
        }
        a.busyT = 0.7f;
        msg(std::string(bd->name) + " CALLS ITS PACK!", C_RED);
        audio.play(S_FELL, 0.5f);
        return;
    }
    switch (a.bossAct) {
        case 0:  // idle: choose the next attack once the cooldown is over and the hunter is in reach
            a.bossCd -= dt;
            if (a.bossCd <= 0 && dp < BOSS_ACT_RANGE && a.stunT <= 0 && a.busyT <= 0) {
                a.bossDir = (p.pos - a.pos).norm();  // locked now, so a sideways dodge-roll gets out of the way
                a.bossHit = false;
                if (dp > 70.f && !rng.chance(0.25f)) { a.bossAct = 1; a.bossT = BOSS_CHARGE_TELE; }
                else { a.bossAct = 3; a.bossT = BOSS_SLAM_TELE; }
                a.busyT = a.bossT;
                audio.play(S_ERROR, 0.4f);
            }
            break;
        case 1:  // charge: winding up
            a.bossT -= dt;
            if (a.bossT <= 0) { a.bossAct = 2; a.bossT = BOSS_CHARGE_TIME; a.busyT = BOSS_CHARGE_TIME; }
            break;
        case 2:  // charge: the rush
            a.bossT -= dt;
            a.busyT = std::max(a.busyT, 0.05f);
            stepAnimal(a, a.bossDir, D.run * BOSS_CHARGE_SPEED, dt);
            a.face = a.bossDir.x >= 0 ? 1 : -1;
            if (!a.bossHit && dist(a.pos, p.pos) < D.hitR * a.scale + 12.f) {
                a.bossHit = true;
                hurtPlayer(D.dmg * BOSS_CHARGE_DMG, a.pos, a.kind);
                pSlowT = std::max(pSlowT, 1.f);
            }
            if (a.bossT <= 0) { a.bossAct = 0; a.bossCd = enraged ? BOSS_COOLDOWN * 0.65f : BOSS_COOLDOWN; a.busyT = 0.35f; }
            break;
        case 3:  // slam: rearing up
            a.bossT -= dt;
            if (a.bossT <= 0) {
                if (dist(a.pos, p.pos) < BOSS_SLAM_RADIUS) {
                    hurtPlayer(D.dmg * BOSS_SLAM_DMG, a.pos, a.kind);
                    pSlowT = std::max(pSlowT, 1.5f);
                }
                shakeAmt = std::max(shakeAmt, 5.f);
                audio.play(S_HIT, 0.8f);
                puff(a.pos, 10, HEX(0xcdb98f));
                a.bossAct = 0;
                a.bossCd = enraged ? BOSS_COOLDOWN * 0.65f : BOSS_COOLDOWN;
                a.busyT = 0.5f;
            }
            break;
    }
}

// red warnings on the ground: a line for the charge, a ring for the slam
void Game::drawBossTelegraph() {
    for (auto& a : animals) {
        if (!a.boss || a.dead || a.gone) continue;
        const float pulse = 0.55f + 0.4f * sinf(timeAcc * 20.f);
        const u32 col = RGBA(230, 56, 40, (int)(200 * pulse));
        const int ax = (int)a.pos.x - camX, ay = (int)a.pos.y - camY;
        if (a.bossAct == 1) {
            for (int i = 1; i <= 12; i++) fillRect(ax + (int)(a.bossDir.x * i * 9.f) - 1, ay + (int)(a.bossDir.y * i * 9.f) - 1, 3, 3, col);
        } else if (a.bossAct == 3) {
            for (int i = 0; i < 24; i++) {
                const float t = i * 6.2832f / 24.f;
                fillRect(ax + (int)(cosf(t) * BOSS_SLAM_RADIUS) - 1, ay + (int)(sinf(t) * BOSS_SLAM_RADIUS * 0.8f) - 1, 3, 3, col);
            }
        }
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
