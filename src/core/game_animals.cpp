// game_animals.cpp - wild animals: spawning, rarity tiers, AI, hits, kills and carcasses.
#include <cstdio>
#include <fstream>
#include <sstream>
#include "core/game.hpp"
#include "core/logic_util.hpp"

// ================================================================= animals
bool Game::trySpawn(int kind, float minDist, bool offscreen) {
    // how close to the village each species may be placed: rabbits live nearest, wolves keep deep in the forest. The far limit is the
    // whole home forest (the thick tree wall of the original area and the forest around it), not a ring around the village.
    static const float nearest[A_COUNT] = {140, 240, 360, 460};
    for (int it = 0; it < 200; it++) {
        const V2 pos(rng.range(4 * TS, (MW - 4) * TS), rng.range(4 * TS, (MH - 4) * TS));
        if (world.biomeAtPos(pos.x, pos.y) != B_HOME) continue;
        if (dist(pos, world.village) < nearest[kind]) continue;
        if (world.blocked(pos.x, pos.y, 9)) continue;
        if (world.inTown(pos.x, pos.y, 48.f)) continue;  // the town (and its tree wall) is no place for wild animals
        if (dist(pos, p.pos) < minDist) continue;
        if (offscreen) {
            float sx = pos.x - cam.x, sy = pos.y - cam.y;
            if (sx > -50 && sx < LW + 50 && sy > -60 && sy < LH + 60) continue;
        }
        Animal a;
        a.kind = kind;
        a.biome = world.biomeAtPos(pos.x, pos.y);
        a.variant = (kind == A_DEER && rng.chance(0.45f)) ? 1 : 0;
        a.pos = pos;
        a.target = pos;
        applyTier(a, rollTier());
        a.face = rng.chance(0.5f) ? 1 : -1;
        a.bias = rng.chance(0.5f) ? 1.f : -1.f;
        a.idleT = rng.range(0, 3);
        a.animT = rng.range(0, 4);
        a.bodyH = ANIMAL[kind].bodyH * a.scale;
        animals.push_back(a);
        return true;
    }
    return false;
}

// ---- rarity tiers. Rolled on frng so the gameplay rng sequence is the same as before the wildlife update.
int Game::rollTier() {
    float u = frng.f();
    float c = TIER[TR_LEGENDARY].chance;
    if (u < c && liveLegendaries() < LEGEND_MAX_ALIVE && legCool <= 0) {
        legCool = LEGEND_COOLDOWN;
        return TR_LEGENDARY;
    }
    // a legendary roll that is not allowed (one already alive / cooling down) simply becomes a rare
    if (u < (c += TIER[TR_RARE].chance)) return TR_RARE;
    if (u < c + TIER[TR_UNCOMMON].chance) return TR_UNCOMMON;
    return TR_COMMON;
}
void Game::applyTier(Animal& a, int tier) {
    const TierDef& T = TIER[tier];
    a.tier = tier;
    spawnStat[a.kind][tier]++;
    a.scale = T.scale;
    a.maxhp = a.hp = std::max(1, (int)roundf(ANIMAL[a.kind].hp * T.hp));
}
int Game::liveLegendaries() const {
    int n = 0;
    for (auto& a : animals)
        if (!a.dead && a.tier == TR_LEGENDARY) n++;
    return n;
}

Animal* Game::spawnAnimal(int kind, V2 pos, int form, int tier) {
    Animal a;
    const AnimalDef& D = ANIMAL[kind];
    a.kind = kind;
    a.biome = world.biomeAtPos(pos.x, pos.y);
    a.variant = form;
    a.pos = a.target = pos;
    applyTier(a, tier);
    if (ren) ensureRichBank(ren, A, kind, form, tier);  // a rarer specimen's sprites are drawn the first time one appears
    a.face = frng.chance(0.5f) ? 1 : -1;
    a.bias = frng.chance(0.5f) ? 1.f : -1.f;
    a.idleT = frng.range(0, 3);
    a.animT = frng.range(0, 4);
    a.bodyH = D.bodyH * a.scale;
    if (D.flags & BH_SOARS) a.air = FLY_HEIGHT;  // soaring birds spawn already airborne; the others start on the ground
    animals.push_back(a);
    return &animals.back();
}

static bool nearDeepWater(const World& w, int tx, int ty, int r) {
    for (int y = ty - r; y <= ty + r; y++)
        for (int x = tx - r; x <= tx + r; x++)
            if (w.water(x, y)) return true;
    return false;
}

// Streaming spawns for the four new biomes: keeps the live population small and local.
void Game::streamSpawns(float dt) {
    for (int k = 0; k < A_COUNT; k++)
        if (spCool[k] > 0) spCool[k] -= dt;
    if (legCool > 0) legCool -= dt;
    spawnT -= dt;
    if (spawnT > 0) return;
    spawnT = SPAWN_TICK;
    // despawn: far away living animals (never carcasses - those fade on their own)
    for (auto& a : animals)
        if (!a.dead && a.biome != B_HOME && dist(a.pos, p.pos) > DESPAWN_DIST) a.gone = true;
    int liveN = 0;
    for (auto& a : animals)
        if (!a.dead && !a.gone && a.biome != B_HOME) liveN++;  // the home population has its own fixed targets
    if (liveN >= MAX_LIVE_ANIMALS || p.dead) return;
    int b = world.biomeAtPos(p.pos.x, p.pos.y);
    if (b == B_HOME) return;
    float night = 1.f - daylight();
    // pick one due species per tick (round-robin start so the table order doesn't bias)
    const auto& tab = SPAWNS[b];
    if (tab.empty()) return;
    int start = frng.irange(0, (int)tab.size() - 1);
    for (size_t j = 0; j < tab.size(); j++) {
        const SpawnDef& S = tab[(start + j) % tab.size()];
        if (spCool[S.kind] > 0) continue;
        int cnt = 0;
        for (auto& a : animals)
            if (!a.dead && !a.gone && a.kind == S.kind && dist(a.pos, p.pos) < SPAWN_AREA) cnt++;
        int cap = S.cap + (night > 0.5f ? (int)S.capNight : 0);
        if (cnt >= cap) continue;
        if (!frng.chance(S.chance)) {
            spCool[S.kind] = 2.f;  // failed roll: try again soon, not on the next tick
            continue;
        }
        for (int it = 0; it < 30; it++) {
            float ang = frng.range(0, 6.2832f), rad = frng.range(SPAWN_MIN, SPAWN_MAX);
            V2 pos = p.pos + V2(cosf(ang) * rad, sinf(ang) * rad * 0.8f);
            if (world.biomeAtPos(pos.x, pos.y) != b) continue;
            if (world.blocked(pos.x, pos.y, 9)) continue;
            int tx = (int)(pos.x / TS), ty = (int)(pos.y / TS);
            if (S.terrain && !(S.terrain & TM(world.tile[ty][tx]))) continue;
            if (S.nearWater && !nearDeepWater(world, tx, ty, 3)) continue;
            float sx = pos.x - cam.x, sy = pos.y - cam.y;
            if (sx > -50 && sx < LW + 50 && sy > -60 && sy < LH + 60) continue;
            int form = (ANIMAL[S.kind].forms > 1 && frng.chance(S.form1)) ? 1 : 0;
            spawnAnimal(S.kind, pos, form, rollTier());
            spCool[S.kind] = S.cooldown;
            return;
        }
        spCool[S.kind] = 2.f;
        return;
    }
}

void Game::populate() {
    animals.clear();
    int counts[A_COUNT] = {HOME_HERDS[0], HOME_HERDS[1], HOME_HERDS[2], HOME_HERDS[3]};
    for (int k = 0; k < A_COUNT; k++)
        for (int i = 0; i < counts[k]; i++) trySpawn(k, 110, false);
}

bool Game::stepAnimal(Animal& a, V2 dir, float speed, float dt) {
    static const float offs[7] = {0, 0.6f, -0.6f, 1.2f, -1.2f, 1.9f, -1.9f};
    float base = atan2f(dir.y, dir.x);
    const AnimalDef& D = ANIMAL[a.kind];
    float r = D.hop ? 3.5f : 5.5f;
    // rabbits move in hops
    float sp = speed;
    if (D.hop) sp *= 0.35f + 0.9f * std::max(0.f, sinf(a.animT * 1.5708f * 2));
    for (int k = 0; k < 7; k++) {
        float ang = base + offs[k] * a.bias;
        V2 d(cosf(ang), sinf(ang));
        V2 np = a.pos + d * (sp * dt);
        if (world.inTown(np.x, np.y, 32.f) && !world.inTown(a.pos.x, a.pos.y, 32.f)) continue;  // wild animals keep out of the town
        if (!world.blocked(np.x, np.y, r) && world.biomeAtPos(np.x, np.y) == a.biome) {
            if (k > 0 && (k % 2 == 0)) a.bias = -a.bias;
            a.pos = np;
            a.vel = d * sp;
            return true;
        }
    }
    a.vel = V2();
    return false;
}

void Game::updateAnimals(float dt) {
    float night = 1.f - daylight();
    // ---- the food chain: predators that are stalking prey this frame, and who counts as their prey
    int hunters[64], nh = 0;
    for (size_t i = 0; i < animals.size() && nh < 64; i++)
        if (!animals[i].dead && !animals[i].gone && animals[i].st == AS_HUNT && ANIMAL[animals[i].kind].preyMask) hunters[nh++] = (int)i;
    // pr is prey of h: same biome, alive, and never a rare or legendary specimen (those are the hunter's trophies)
    auto preyOf = [&](const Animal& pr, const Animal& h) {
        return &pr != &h && !pr.dead && !pr.gone && pr.tier < TR_RARE && pr.biome == h.biome && ((ANIMAL[h.kind].preyMask >> pr.kind) & 1u);
    };
    // v bolts from `from` (a predator), and its herd mates close by bolt with it
    auto fleePredator = [&](Animal& v, V2 from) {
        for (auto& o : animals) {
            if (o.dead || o.gone || o.kind != v.kind || o.biome != v.biome) continue;
            if (&o != &v && (dist(o.pos, v.pos) > 75.f || (o.st != AS_WANDER && o.st != AS_FEED && o.st != AS_REST))) continue;
            o.st = AS_FLEE;
            o.fleeAnimal = true;
            o.threat = from;
            o.stT = frng.range(2.5f, 4.f);
            o.alertT = 0;
        }
    };
    // a startled animal warns the same species close by: they bolt from the hunter too
    auto alarmHerd = [&](Animal& v) {
        for (auto& o : animals) {
            if (&o == &v || o.dead || o.gone || o.kind != v.kind || o.biome != v.biome || dist(o.pos, v.pos) > 70.f) continue;
            if (o.st != AS_WANDER && o.st != AS_FEED && o.st != AS_REST) continue;
            if (ANIMAL[o.kind].aggro) continue;  // fighters do not scatter
            o.st = AS_FLEE;
            o.fleeAnimal = false;
            o.stT = frng.range(2.5f, 4.f);
        }
    };
    for (auto& a : animals) {
        if (a.st != AS_FLEE) a.fleeAnimal = false;
        if (a.huntCd > 0) a.huntCd -= dt;
        if (a.flash > 0) a.flash -= dt;
        if (a.hpShow > 0) a.hpShow -= dt;
        if (a.atkCd > 0) a.atkCd -= dt;
        // knockback dies away fast on a living animal; a dying one keeps its momentum and slides to a stop (friction, not a cut-off)
        if (a.kb.len() > (a.dead ? 2.f : 3.f)) {
            const V2 from = a.pos;
            moveEnt(world, a.pos, a.kb * dt, 5.f);
            if (a.dead && a.kb.len() > 25.f && rng.chance(dt * 7.f)) decals.push_back({a.pos + V2(rng.range(-2, 2), 2), 0, 0, 40});  // a smear along the slide
            if (a.dead && dist(from, a.pos) < a.kb.len() * dt * 0.3f) a.kb = a.kb * 0.5f;  // ran into something: scrub the speed off
            a.kb = a.kb * expf((a.dead ? -3.4f : -9.f) * dt);
        } else
            a.kb = V2();
        if (a.dead) {
            a.deadT += dt;
            updateAnimalAnim(a, dt);
            if (a.deadT > 170) a.fade -= dt / 8.f;
            if (a.totalRemain <= 0) a.fade -= dt * 1.6f;
            if (a.fade <= 0) a.gone = true;
            continue;
        }
        const AnimalDef& D = ANIMAL[a.kind];
        V2 dp = p.pos - a.pos;
        float dd = p.dead ? 9999.f : dp.len();
        const TierDef& T = TIER[a.tier];
        // night hunters (cats, coyotes) are skittish in daylight
        bool agg = D.aggro && (!(D.flags & BH_NIGHT_AGGRO) || night > 0.45f);
        float notice = D.notice * T.notice * (D.aggro ? 1.f + 0.3f * night : 1.f);
        a.stT -= dt;
        a.vel = V2();
        if (a.alertT > 0) {  // startled: freeze and look at the hunter, then bolt (the state change was made when it noticed)
            a.alertT -= dt;
            if (dd < 9000.f) a.face8 = facingFromDir(dp.x, dp.y, a.face8);
            updateAnimalAnim(a, dt);
            continue;
        }
        switch (a.st) {
            case AS_WANDER:
            case AS_FEED:
            case AS_REST: {
                bool alert = dd < (D.aggro ? notice * D.alertMul : notice);
                if (!alert)
                    for (int hi = 0; hi < nh; hi++) {  // prey notice a predator that is hunting close by
                        const Animal& h = animals[hunters[hi]];
                        if (preyOf(a, h) && dist(a.pos, h.pos) < D.notice * T.notice * 0.9f) {
                            fleePredator(a, h.pos);
                            break;
                        }
                    }
                if (a.st == AS_FLEE) break;
                if (a.st != AS_WANDER) {
                    if (a.st == AS_REST) a.vel = V2();
                    if (!alert && a.stT <= 0) {
                        a.st = AS_WANDER;
                        a.idleT = frng.range(0.5f, 2.f);
                    }
                    if (!alert) break;
                }
                if (alert) {
                    if (agg) {
                        a.st = AS_CHASE;
                        a.stT = 12;
                        ftext(a.pos + V2(0, -a.bodyH - 12), "!", C_RED, 0.8f);
                    } else {
                        a.st = AS_FLEE;
                        a.stT = rng.range(2.5f, 4.f);
                        alarmHerd(a);
                        if (const AnimalClips* ac = animalClips(a.kind))
                            if (ac->poses[AN_ALERT] > 0) a.alertT = ac->clip[AN_ALERT].total();  // a moment of alert before it bolts
                        ftext(a.pos + V2(0, -a.bodyH - 12), "!", C_TEXT, 0.7f);
                    }
                    break;
                }
                if (a.st == AS_WANDER && D.preyMask && a.huntCd <= 0) {  // a predator that sees prey may stalk it
                    bool sees = false;
                    for (auto& pr : animals)
                        if (preyOf(pr, a) && dist(pr.pos, a.pos) < D.notice * T.notice * 0.9f) {
                            sees = true;
                            break;
                        }
                    if (sees && frng.chance(dt * 0.6f)) {
                        a.st = AS_HUNT;
                        a.stT = 10.f;
                        break;
                    }
                }
                if (a.idleT > 0) {
                    a.idleT -= dt;
                    if (a.idleT <= 0 && D.restChance > 0 && frng.chance(D.restChance + D.feedChance)) {
                        bool rest = frng.chance(D.restChance / (D.restChance + D.feedChance));
                        a.st = rest ? AS_REST : AS_FEED;
                        a.stT = frng.range(4.f, 9.f);
                        a.idleT = 0;
                    }
                } else {
                    V2 to = a.target - a.pos;
                    if (to.len() < 5 || a.stT < -7) {
                        a.idleT = rng.range(1.2f, 4.5f);
                        a.target = a.pos + V2(rng.range(-90, 90), rng.range(-70, 70));
                        a.stT = 0;
                    } else if (!stepAnimal(a, to.norm(), D.wander * T.speed, dt)) {
                        a.target = a.pos + V2(rng.range(-60, 60), rng.range(-50, 50));
                    }
                }
                break;
            }
            case AS_FLEE: {
                V2 src = p.pos;
                bool hunted = false;  // a predator is still close (when fleeing from one)
                if (a.fleeAnimal) {
                    float best = D.notice * T.notice * 1.5f;
                    src = a.threat;
                    for (int hi = 0; hi < nh; hi++) {
                        const Animal& h = animals[hunters[hi]];
                        const float d = dist(a.pos, h.pos);
                        if (preyOf(a, h) && d < best) best = d, src = h.pos, hunted = true;
                    }
                    a.threat = src;
                }
                V2 away = (a.pos - src).norm();
                if (away.len() < 0.1f) away = V2(1, 0);
                V2 perp(-away.y, away.x);
                float j = sinf(timeAcc * 2.3f + a.animT) * 0.35f;
                V2 dir = (away + perp * j).norm();
                stepAnimal(a, dir, D.run * T.speed * (a.hp < a.maxhp ? 1.08f : 1.f), dt);
                if (a.stT <= 0 && (a.fleeAnimal ? !hunted : dd > notice * 1.4f)) {
                    a.st = AS_WANDER;
                    a.idleT = 1;
                    a.target = a.pos;
                }
                break;
            }
            case AS_HUNT: {
                if (dd < (D.aggro ? notice * D.alertMul : notice)) {  // the hunter outranks any prey
                    a.st = agg ? AS_CHASE : AS_FLEE;
                    a.stT = agg ? 12.f : rng.range(2.5f, 4.f);
                    a.fleeAnimal = false;
                    break;
                }
                int best = -1;
                float bd = 220.f;
                for (size_t j = 0; j < animals.size(); j++) {
                    if (!preyOf(animals[j], a)) continue;
                    const float d = dist(animals[j].pos, a.pos);
                    if (d < bd) bd = d, best = (int)j;
                }
                if (best < 0 || a.stT <= 0) {  // nothing left to chase, or out of breath: rest before the next hunt
                    a.st = AS_WANDER;
                    a.huntCd = frng.range(30.f, 70.f);
                    a.idleT = 1.5f;
                    a.target = a.pos;
                    break;
                }
                Animal& pr = animals[best];
                const V2 toPrey = (pr.pos - a.pos).norm();
                if (bd < D.hitR + 8.f) {
                    if (a.atkCd <= 0) {  // a bite
                        pr.hp -= std::max(1, (int)lroundf(D.dmg * T.dmg * 0.7f));
                        pr.flash = 0.16f;
                        pr.kb += toPrey * 60.f;
                        pr.alertT = 0;
                        bloodBurst(pr.pos + V2(0, -pr.bodyH), toPrey, 5);
                        if (const AnimalClips* ac = animalClips(a.kind)) a.atkT = ac->clip[AN_ATTACK].total();
                        if (const AnimalClips* pc = animalClips(pr.kind)) {
                            pr.hurtT = pc->clip[AN_HURT].total();
                            pr.anim.play(AN_HURT, &pc->clip[AN_HURT], true);
                        }
                        a.atkCd = D.atkCd * 1.2f;
                        a.face8 = facingFromDir(toPrey.x, toPrey.y, a.face8);
                        if (pr.hp <= 0) {  // the kill: the carcass stays for the hunter to find; the predator feeds and then rests
                            killAnimal(pr, false);
                            a.st = AS_FEED;
                            a.stT = frng.range(6.f, 10.f);
                            a.huntCd = frng.range(50.f, 90.f);
                        } else
                            fleePredator(pr, a.pos);
                    }
                } else
                    stepAnimal(a, toPrey, D.run * T.speed * 0.92f, dt);
                break;
            }
            case AS_CHASE: {
                V2 toP = dp.norm();
                if (D.fleeHp > 0 && a.hp < a.maxhp * D.fleeHp) {
                    a.st = AS_FLEE;
                    a.stT = 6;
                    break;
                }
                if (dd > D.leash || p.dead || !agg) {
                    a.st = AS_WANDER;
                    a.idleT = 1.5f;
                    a.target = a.pos;
                    break;
                }
                a.face = dp.x > 0 ? 1 : -1;
                if (dd < D.hitR + 9) {
                    if (a.atkCd <= 0) {
                        hurtPlayer(D.dmg * T.dmg, a.pos, a.kind);
                        if (const AnimalClips* ac = animalClips(a.kind)) a.atkT = ac->clip[AN_ATTACK].total();
                        a.atkCd = D.atkCd;
                        a.kb = toP * -70;
                    }
                } else
                    stepAnimal(a, toP, D.run * T.speed * (dd < 50 ? 1.12f : 1.f), dt);
                break;
            }
        }
        if (fabsf(a.vel.x) > 2.f && a.st != AS_CHASE) a.face = a.vel.x > 0 ? 1 : -1;
        else if (fabsf(a.vel.x) > 2.f) a.face = a.vel.x > 0 ? 1 : -1;
        float spd = a.vel.len();
        if (spd > 1) a.animT += spd * dt * D.animK;
        a.noticeT += dt;
        const int prevAnim = a.anim.state;
        updateAnimalAnim(a, dt);
        if (animalClips(a.kind) && a.air < 1.f) {  // dust kicked up by a running animal, and a puff as a bird launches itself
            if (a.anim.state == AN_RUN && rng.chance(dt * 9.f)) puff(a.pos + V2(-a.vel.x * 0.04f, -1), 1, HEX(0xcdb98f));
            if (a.anim.state == AN_TAKEOFF && prevAnim != AN_TAKEOFF) puff(a.pos + V2(0, -1), 4, HEX(0xd8c8a8));
        }
    }
    animals.erase(std::remove_if(animals.begin(), animals.end(), [](const Animal& a) { return a.gone; }), animals.end());
}

// Choose and advance the animation of an animal that has a full set (animal_anim.hpp). The AI decides what the animal does; this
// only reads it: death, a fresh hit, a bite landing, running (fleeing / charging), walking, idling. Walk and run play at the pace
// of the animal's real speed so the feet do not slide.
void Game::updateAnimalAnim(Animal& a, float dt) {
    const AnimalClips* ac = animalClips(a.kind);
    if (!ac) return;
    const AnimalDef& D = ANIMAL[a.kind];
    const float spd = a.vel.len();
    a.hurtT = std::max(0.f, a.hurtT - dt);
    a.atkT = std::max(0.f, a.atkT - dt);
    int st;
    float rate = 1.f;
    // flyers climb to FLY_HEIGHT while fleeing (soarers while moving about) and come down to rest or feed: air eases between the two
    int flight = 0;  // 1 climbing, 2 cruising, 3 descending
    if ((D.flags & BH_FLIES) && !a.dead && a.alertT <= 0) {
        const bool soar = (D.flags & BH_SOARS) && a.st != AS_REST && a.st != AS_FEED && (spd > 1.f || a.st == AS_CHASE);
        const float goal = (a.st == AS_FLEE || soar) ? FLY_HEIGHT : 0.f;
        if (a.air < goal) a.air = std::min(goal, a.air + FLY_CLIMB * dt);
        else if (a.air > goal) a.air = std::max(goal, a.air - FLY_DESCEND * dt);
        flight = a.air < goal ? 1 : (a.air > goal ? 3 : (a.air > 0.5f ? 2 : 0));
    }
    if (a.dead) st = AN_DEATH;
    else if (a.hurtT > 0) st = AN_HURT;
    else if (a.atkT > 0) st = AN_ATTACK;
    else if (a.alertT > 0) st = AN_ALERT;
    else if (flight == 1) st = (a.anim.state == AN_TAKEOFF && a.anim.done) ? AN_FLY : AN_TAKEOFF;
    else if (flight == 2) st = AN_FLY;
    else if (flight == 3) st = AN_LAND;  // holds its last pose until the feet touch down
    else if (spd > 1.f) {
        const bool fast = a.st == AS_CHASE || a.st == AS_FLEE || a.st == AS_HUNT;
        st = fast ? AN_RUN : AN_WALK;
        rate = clampf(spd / (fast ? D.run : D.wander), 0.5f, 1.8f);
    } else if (a.st == AS_FEED && ac->poses[AN_EAT] > 0) st = AN_EAT;
    else if (a.st == AS_REST && ac->poses[AN_REST] > 0) st = AN_REST;
    else st = AN_IDLE;
    if (!a.dead) {
        if (a.st == AS_CHASE && dist(a.pos, p.pos) < D.hitR + 30) a.face8 = facingFromDir(p.pos.x - a.pos.x, p.pos.y - a.pos.y, a.face8);
        else if (spd > 1.f) a.face8 = facingFromDir(a.vel.x, a.vel.y, a.face8);
    }
    a.anim.play(st, &ac->clip[st]);
    a.anim.update(dt * rate);
}

void Game::hitAnimal(Animal& a, float dmg, V2 dir, bool crit) {
    int d = std::max(1, (int)roundf(dmg));
    a.hp -= d;
    a.alertT = 0;  // a hit ends any startle: it bolts at once
    a.flash = 0.16f;
    a.hpShow = 3.0f;
    if (const AnimalClips* ac = animalClips(a.kind)) {
        a.hurtT = ac->clip[AN_HURT].total();
        a.atkT = 0;
        a.anim.play(AN_HURT, &ac->clip[AN_HURT], true);  // every hit restarts the flinch
    }
    a.kb += dir * (crit ? 150.f : 105.f);
    bloodBurst(a.pos + V2(0, -a.bodyH), dir, crit ? 14 : 8);
    ftext(a.pos + V2(0, -a.bodyH - 10), (crit ? std::string("CRIT ") : std::string()) + S(d), crit ? C_ORANGE : C_TEXT, 0.9f);
    audio.play(crit ? S_CRIT : S_HIT);
    shakeAmt = std::max(shakeAmt, crit ? 3.f : 1.5f);
    decals.push_back({a.pos + V2(rng.range(-5, 5), rng.range(0, 4)), 0, 0, 40});
    const AnimalDef& D = ANIMAL[a.kind];
    bool agg = D.aggro && (!(D.flags & BH_NIGHT_AGGRO) || daylight() < 0.55f);
    if (agg) {
        a.st = AS_CHASE;
        a.stT = 10;
    } else {
        a.st = AS_FLEE;
        a.stT = 7;
        a.fleeAnimal = false;  // it flees from the hunter now
    }
    if (a.hp <= 0) killAnimal(a);
}

void Game::killAnimal(Animal& a, bool byHunter) {
    a.dead = true;
    a.deadT = 0;
    a.hp = 0;
    {  // the death slide: the killing blow's push plus what is left of the animal's own run carries the body along
        const V2 carry = a.vel * 0.7f;
        V2 m = a.kb + carry;
        const float ml = m.len();
        if (ml > 1e-3f) a.kb = m * (std::min(std::max(ml, 80.f), 190.f) / ml);
    }
    a.vel = V2();
    if (byHunter) {
        kills++;
        questEvent(Q_KILL, a.kind, 1);
    }
    Rng& dropRng = byHunter ? rng : frng;  // a predator's kill must not disturb the original random sequence
    const AnimalDef& D = ANIMAL[a.kind];
    int total = 0;
    for (int m = 0; m < M_COUNT; m++) {
        a.remain[m] = D.drop[m][1] > 0 ? dropRng.irange(D.drop[m][0], D.drop[m][1]) : 0;
        total += a.remain[m];
    }
    if (D.formDrop[0] >= 0 && a.variant == 1) {  // e.g. a buck's antlers
        int n = dropRng.irange(D.formDrop[1], D.formDrop[2]);
        a.remain[D.formDrop[0]] += n;
        total += n;
    }
    if (a.tier > 0) {  // better specimens carry more
        total = 0;
        for (int m = 0; m < M_COUNT; m++) {
            a.remain[m] = (int)ceilf(a.remain[m] * TIER[a.tier].drops);
            total += a.remain[m];
        }
        if (a.tier == TR_LEGENDARY && D.legendDrop[0] >= 0) {
            int n = frng.irange(D.legendDrop[1], D.legendDrop[2]);
            a.remain[D.legendDrop[0]] += n;
            total += n;
        }
        if (byHunter && TIER[a.tier].bounty > 0) {
            p.coins += TIER[a.tier].bounty;
            ftext(a.pos + V2(0, -a.bodyH - 30), "+" + S(TIER[a.tier].bounty) + "C", C_GOLD, 1.6f);
        }
        if (byHunter) msg(std::string(TIER[a.tier].name) + " " + animalName(a.kind, a.tier) + " HUNTED!", TIER[a.tier].col);
    }
    a.air = 0;
    if (total == 0) {
        a.remain[M_MEAT] = 1;
        total = 1;
    }
    a.totalRemain = total;
    decals.push_back({a.pos + V2(0, 3), D.decal, 0, 80});
    bloodBurst(a.pos + V2(0, -a.bodyH), a.kb.norm(), 16);
    if (!byHunter) return;  // killed by a predator: the carcass is there to find, but there is no kill credit, fanfare or XP
    audio.play(S_KILL);
    shakeAmt = std::max(shakeAmt, 4.f);
    ftext(a.pos + V2(0, -a.bodyH - 18), "HUNTED!", C_GOLD, 1.4f);
    // skill XP for a completed kill (every kill is made with a thrown weapon or a launcher); bigger and rarer game pays more
    const float tierMul = a.tier > 0 ? TIER[a.tier].drops : 1.f;
    const int rec = 5 + (int)(D.hp / 3);  // level at which this animal is no longer a challenge
    grantXp(SK_HUNTING, std::max(1, (int)lroundf((3 + D.hp / 6.f) * tierMul * xpFalloff(p.sk.level(SK_HUNTING), rec))));
    grantXp(SK_RANGED, std::max(1, (int)lroundf((2 + D.hp / 8.f) * tierMul * xpFalloff(p.sk.level(SK_RANGED), rec))));
}
