// game_player.cpp - the hunter: damage, throwing, gathering, movement and animation, projectiles.
#include <cstdio>
#include <fstream>
#include <sstream>
#include "core/game.hpp"
#include "core/logic_util.hpp"

// ================================================================= player
void Game::hurtPlayer(float dmg, V2 from, int srcKind) {
    if (p.invuln > 0 || p.dead) return;
    dmg *= 1.f - skillBonus(SK_DEFENSE);
    if (p.has(CG_VENOMWRAPS) && (srcKind == A_RATTLER || srcKind == A_MOCCASIN)) dmg *= 0.65f;  // snake-proof wraps
    int d = std::max(1, (int)ceilf(dmg * (1.f - std::min(0.9f, ARMOR_RED[p.up[U_ARMOR]] * qualityDef(p.armorQ).stat))));
    p.hp -= d;
    if (p.hp > 0 && p.up[U_ARMOR] > 0) grantXp(SK_DEFENSE, std::max(1, d / 8));  // surviving a hit in armor
    p.invuln = 0.85f;
    p.sinceHurt = 0;
    V2 dir = (p.pos - from).norm();
    p.kb = dir * 140;
    shakeAmt = 5.5f;
    audio.play(S_HURT);
    ftext(p.pos + V2(0, -24), "-" + S(d), C_RED, 1.0f);
    bloodBurst(p.pos + V2(0, -8), dir, 9);
    if (p.hp <= 0) {
        p.hp = 0;
        p.dead = true;
        p.deadT = 0;
        p.charging = false;
        int loss = (int)(p.coins * 0.15f);
        p.coins -= loss;
        msg("YOU WERE KNOCKED OUT!  LOST " + S(loss) + " COINS", C_RED);
    }
}

// A weapon can be used when you own it and (for the bow and crossbow) have ammo for it.
// a weapon can be used when it is equipped on the hotbar, there is one in the pack and (for launchers) there is ammunition
bool Game::canUse(int w) const { return p.equippedWpn(w) && p.wpn(w) > 0 && (!isLauncher(w) || p.ammo(WPN[w].ammo) > 0); }

void Game::throwWeapon() {
    int w = p.sel;
    if (!canUse(w)) return;
    const WpnDef& D = WPN[w];
    const bool launcher = isLauncher(w);
    // the best piece is used: a launcher is not used up (its ammunition is), a thrown weapon is
    const int wq = p.inv.bestQuality(itemWpn(w));
    const int aq = launcher ? p.inv.bestQuality(itemAmmo(D.ammo)) : 0;
    if (launcher) p.inv.takeBest(itemAmmo(D.ammo));
    else p.inv.takeBest(itemWpn(w));
    V2 hand = p.pos + V2(0, -10);
    V2 mw(in.mx + camX, in.my + camY);
    V2 d = mw - hand;
    float dl = d.len();
    V2 dir = dl > 4 ? d * (1.f / dl) : p.aim;
    float c = p.charge;
    Proj pr;
    pr.pos = hand + dir * 8;
    pr.last = pr.pos;
    pr.dir = dir;
    pr.speed = D.speed * (1 + 0.15f * c);
    pr.range = std::min(D.range * (1 + 0.15f * c), dl + 24);
    pr.travelled = 0;
    pr.wpn = w;
    pr.angle = atan2f(dir.y, dir.x);
    const bool spearLike = w == W_JAVELIN || w == W_SPEAR || w == W_IRONSPEAR || w == W_RSPEAR || w == W_SUNJAV || w == W_BOGSPEAR || w == W_HARPOON || w == W_STEELSPEAR || w == W_FROSTBRAND;
    const bool axeLike = w == W_AXE || w == W_HVYAXE || w == W_CLIFFAXE;
    pr.spin = (spearLike || launcher) ? 0.f : (axeLike ? 17.f : 26.f);
    const float biomeMul = (D.biome >= 0 && curBiome == D.biome) ? D.biomeMul : 1.f;  // biome weapons: +30% at home
    pr.crit = rng.chance(0.10f);
    pr.dmg = D.dmg * biomeMul * (1.f + skillBonus(SK_HUNTING)) * (1 + 0.4f * c) * rng.range(0.9f, 1.1f) * (pr.crit ? 1.6f : 1.f);
    if (launcher) {
        // a bow shot is only as good as the draw; the crossbow is always fully loaded when it fires
        float draw = D.draw > 0 ? c : 1.f;
        pr.speed = D.speed * (0.55f + 0.45f * draw);
        pr.range = std::min(D.range * (0.5f + 0.5f * draw), dl + 24);
        pr.dmg = D.dmg * biomeMul * (1.f + skillBonus(SK_HUNTING)) * (0.45f + 0.55f * draw) * rng.range(0.92f, 1.08f) * (pr.crit ? 1.6f : 1.f);
        pr.ammo = D.ammo;
    }
    pr.q = wq;
    pr.aq = aq;
    // quality: the weapon's full bonus, and half of the ammunition's
    pr.dmg *= qualityDef(wq).stat * (launcher ? 1.f + (qualityDef(aq).stat - 1.f) * 0.5f : 1.f);
    if (p.has(CG_CROWN)) pr.dmg *= 1.15f;  // the crown of the Frostbound King
    projs.push_back(pr);
    p.cd = D.cd;
    p.throwAnim = 0.3f;
    audio.play(S_THROW, 0.6f);
    if (!canUse(w) && !launcher) {  // ran out of thrown weapons: switch to another one you can use
        for (int i = 0; i < W_COUNT; i++)
            if (canUse(i)) {
                p.sel = i;
                break;
            }
    }
}


void Game::gatherStep(Animal& c) {
    int m = -1;
    for (int i = 0; i < M_COUNT; i++) {
        int mm = GATHER_ORDER[i];
        if (c.remain[mm] > 0) {
            m = mm;
            break;
        }
    }
    if (m < 0) {
        c.totalRemain = 0;
        return;
    }
    if (!p.inv.canAdd(itemMat(m), 1)) {
        notifyBlocked(itemMat(m), 1);
        return;
    }
    c.remain[m]--;
    c.totalRemain--;
    p.inv.add(itemMat(m), 1);
    int extra = 0;
    const bool skinPiece = m == M_HIDE || m == M_FUR || m == M_SCALE || isTrophy(m);
    if (rng.chance(TOOL_BONUS[p.up[U_TOOL]] + skillBonus(skinPiece ? SK_SKINNING : SK_BUTCHERING)) && p.inv.canAdd(itemMat(m), 1)) {
        p.inv.add(itemMat(m), 1);
        extra = 1;
    }
    questEvent(Q_GATHER, m, 1 + extra);
    {  // butchering for meat, bone and fat; skinning for hide, fur, scales and the trophy pieces
        grantXp(skinPiece ? SK_SKINNING : SK_BUTCHERING, std::min(6, std::max(1, MAT_PRICE[m] / 5)) * (1 + extra));
    }
    ftext(c.pos + V2(rng.range(-6, 6), -10), "+" + S(1 + extra) + " " + MAT_NAME[m], extra ? C_GOLD : C_GREEN, 1.2f);
    bloodBurst(c.pos + V2(0, -4), V2(rng.range(-1, 1), -0.6f), 4);
    puff(c.pos + V2(0, -2), 3, HEX(0xd8c8a8));
    audio.play(S_GATHER);
    if (c.totalRemain <= 0) {
        ftext(c.pos + V2(0, -22), "CLEANED", C_TEXT, 1.0f);
        audio.play(S_PICKUP, 0.4f);
    }
}

void Game::updatePlayer(float dt) {
    Player& P = p;
    float newMax = (float)ARMOR_HP[P.up[U_ARMOR]];
    if (newMax > P.maxhp) P.hp += newMax - P.maxhp;
    P.maxhp = newMax;
    if (P.dead) {
        P.deadT += dt;
        updateHunterAnim(dt, false, false, false, false, -1, 0);
        if (P.deadT > 3.4f) {
            if (delve >= 0) leaveDelve();  // knocked out underground: carried back out to the entrance (the king keeps his health only if he lives)
            else P.pos = world.spawn;
            P.hp = P.maxhp;
            P.dead = false;
            P.invuln = 2.f;
            P.kb = V2();
            for (auto& a : animals)
                if (!a.dead && a.st == AS_CHASE) {
                    a.st = AS_WANDER;
                    a.idleT = 2;
                }
            setCam(0, true);
        }
        return;
    }
    P.cd = std::max(0.f, P.cd - dt);
    P.invuln = std::max(0.f, P.invuln - dt);
    P.sinceHurt += dt;
    P.throwAnim = std::max(0.f, P.throwAnim - dt);
    P.useT = std::max(0.f, P.useT - dt);

    if (in.selectKey >= 0 && in.selectKey < HOT_N) hotUse(in.selectKey);  // number keys = hotbar slots
    if (in.wheel != 0) {  // the wheel moves through every item on the hotbar (weapons, tools, seeds, food...) and takes it in hand
        int occ[HOT_N], n = 0, at = -1;
        const int cur = P.hotSel >= 0 && P.hot[P.hotSel] >= 0 ? P.hotSel : (P.equippedWpn(P.sel) ? P.hotSlotOf(itemWpn(P.sel)) : -1);
        for (int i = 0; i < HOT_N; i++)
            if (P.hot[i] >= 0) {
                if (i == cur) at = n;
                occ[n++] = i;
            }
        if (n > 0) hotSelect(occ[((at < 0 ? 0 : at) + (in.wheel > 0 ? n - 1 : 1)) % n], false);
    }
    fixSel();

    V2 mw(in.mx + camX, in.my + camY);
    V2 hand = P.pos + V2(0, -10);
    V2 aimv = mw - hand;
    if (aimv.len() > 1) P.aim = aimv.norm();

    Animal* c = nearestCarcass(26);
    bool wantGather = in.e && c != nullptr && !P.charging;
    // E priority: a carcass, then a placed structure (chest / campfire), then a tree, plant or rock within reach
    const bool structBusy = !c && structInteract(dt);
    HTarget ht;
    if (!c && !structBusy && !hsOutranksHarvest()) findHarvest(ht);
    bool harvestOk = false;
    if (ht.valid) {
        const ResDef& RD = RES[ht.res];
        const bool toolOk = RD.tool < 0 || bestTool(RD.tool) >= RD.minTier;
        harvestOk = toolOk && p.sk.level(SK_MINING) >= RD.minLv;
        if (!harvestOk && in.ePressed && hvMsgT <= 0) {
            msg(toolOk ? std::string("NEED MINING LEVEL ") + S(RD.minLv) + " FOR " + RD.name : std::string("NEED A ") + TOOLDEF[RD.tool * 4 + RD.minTier - 1].name + " FOR " + RD.name, C_RED);
            audio.play(S_ERROR, 0.5f);
            hvMsgT = 1.5f;
        }
    }
    // being hit knocks you out of the swing; charging a throw pauses it (combat always wins)
    const bool wantHarvest = in.e && ht.valid && harvestOk && !P.charging && P.sinceHurt > 0.5f;

    V2 mv((in.right ? 1.f : 0.f) - (in.left ? 1.f : 0.f), (in.down ? 1.f : 0.f) - (in.up ? 1.f : 0.f));
    bool moving = mv.len() > 0.1f;
    if (moving) mv = mv.norm();
    float spd = 70.f * (1.f + (BOOT_MUL[P.up[U_BOOTS]] - 1.f) * qualityDef(P.bootsQ).stat) * (P.swiftT > 0 ? 1.15f : 1.f);
    const uint8_t groundT = world.tileAt(P.pos.x, P.pos.y);
    { float ts = tileSpeed(groundT); if (ts < 0.93f && p.has(CG_WADERS) && (curBiome == B_SWAMP)) ts = 0.93f; spd *= ts; }  // sand, mud, deep snow slow you down
    float drain = 1.f, regen = 1.f;
    climateMods(drain, regen);
    bool sprinting = false;
    if (in.sprint && moving && !P.exhausted && P.stamina > 0 && !wantGather && !wantHarvest) {
        sprinting = true;
        spd *= 1.55f;
        P.stamina -= 24 * dt * drain;
        P.staDelay = 0.7f;
        if (P.stamina <= 0) {
            P.stamina = 0;
            P.exhausted = true;
        }
    } else {
        P.staDelay -= dt;
        if (P.staDelay <= 0) P.stamina = std::min(100.f, P.stamina + (moving ? 16.f : 32.f) * dt * regen);
        if (P.exhausted && P.stamina > 30) P.exhausted = false;
    }
    if (P.charging) spd *= 0.6f;
    if (pSlowT > 0) spd *= 0.65f;  // frost-slowed
    if (wantGather || wantHarvest) spd = 0;
    V2 want = mv * spd;
    if (groundT == T_ICE) {  // slippery: velocity eases towards the wanted velocity instead of snapping to it
        P.iceV = P.iceV + (want - P.iceV) * (1.f - expf(-2.4f * dt));
        want = P.iceV;
    } else
        P.iceV = want;
    V2 vel = want + P.kb;
    P.kb = P.kb * expf(-9 * dt);
    moveEnt(world, P.pos, vel * dt, 4.f);
    P.pos.x = clampf(P.pos.x, 20, world.pixW() - 20);
    P.pos.y = clampf(P.pos.y, 20, world.pixH() - 20);

    auto setDir = [&](V2 d) {
        P.face8 = facingFromDir(d.x, d.y, P.face8);
        // the facing as the other systems use it (placement, the salon): 0 down, 1 up, 2 side
        const FacingView fv = facingView(P.face8);
        P.dir = fv.view == 0 ? 0 : fv.view == 2 ? 1 : 2;
        if (P.dir == 2) P.flip = fv.mirror;
    };
    if (wantGather) setDir(c->pos - P.pos);
    else if (wantHarvest) setDir(ht.pos - P.pos);
    else if (P.charging || P.throwAnim > 0) setDir(P.aim);
    else if (moving) setDir(mv);
    if (moving && !wantGather && !wantHarvest) {
        P.animT += dt * (sprinting ? 12.f : 8.5f);
        const int half = (int)(P.animT / 3.14159f);  // one footstep per half walk cycle
        if (half != lastStep) {
            lastStep = half;
            playFootstep();
        }
    }
    {
        int swingTool = -1, resCls = -1;
        float swingFrac = 0;
        if (wantHarvest) {
            int ti = -1;
            if (RES[ht.res].tool >= 0 && bestTool(RES[ht.res].tool, &ti) > 0) swingTool = ti;
            resCls = RES[ht.res].cls;
            swingFrac = clampf(P.gatherT / RES[ht.res].swing, 0.f, 1.f);  // the swing's progress: the strike lands when it reaches 1
        }
        updateHunterAnim(dt, moving, sprinting, wantGather, wantHarvest, swingTool, swingFrac, resCls);
    }
    if (sprinting && rng.chance(0.25f)) puff(P.pos + V2(0, -1), 1, HEX(0xcdb98f));

    // --- a farm tool in hand: the click works the soil instead of throwing
    if (P.hand >= 0 && in.lPressed && !wantGather) {
        if (P.hand == HAND_ITEM) hotClick();
        else if (inHomestead() && interior < 0) hsToolUse();
        else msg("FARM TOOLS WORK AT YOUR HOMESTEAD - PRESS A WEAPON KEY TO THROW", C_DIM);
        in.lPressed = false;
    }
    // --- throwing
    if (in.lPressed && !wantGather && P.cd <= 0 && P.hand < 0) {
        if (canUse(P.sel)) {
            const WpnDef& SW = WPN[P.sel];
            if (isLauncher(P.sel) && SW.draw <= 0.f) {  // crossbow: loaded, so it fires at once and then needs reloading
                P.charge = 1.f;
                throwWeapon();
                P.charge = 0;
            } else {
                P.charging = true;
                P.charge = 0;
            }
        } else if (P.wpn(P.sel) > 0 && isLauncher(P.sel) && noAmmoT <= 0) {
            ftext(P.pos + V2(0, -24), std::string("NO ") + AMMO[WPN[P.sel].ammo].name + "S", C_RED, 1.0f);
            audio.play(S_ERROR, 0.5f);
            noAmmoT = 0.8f;
        }
    }
    if (noAmmoT > 0) noAmmoT -= dt;
    if (P.charging) {
        if (!canUse(P.sel)) P.charging = false;
        else {
            const float drawT = WPN[P.sel].draw > 0 ? WPN[P.sel].draw : 0.5f;
            P.charge = std::min(1.f, P.charge + dt / drawT);
            if (in.lReleased || !in.l) {
                throwWeapon();
                P.charging = false;
                P.charge = 0;
            }
        }
    }

    // --- pick thrown weapons back up
    for (size_t i = 0; i < items.size();) {
        GroundItem& it = items[i];
        if (it.item >= 0) {  // a stack the hunter dropped: taken back as far as slots and weight allow
            if (it.lock > 0) it.lock -= dt;
            else if (dist(P.pos, it.pos) < 13) {
                const int got = P.inv.add(it.item, it.n, it.q);
                if (got > 0) {
                    it.n -= got;
                    ftext(P.pos + V2(0, -24), "+" + S(got) + " " + item(it.item).name, C_BLUE, 0.9f);
                    audio.play(S_PICKUP, 0.4f);
                } else
                    notifyBlocked(it.item, 1);
                if (it.n <= 0) {
                    items.erase(items.begin() + i);
                    continue;
                }
            }
            i++;
            continue;
        }
        if (it.ammo >= 0) {
            if (dist(P.pos, it.pos) < 13 && P.inv.canAdd(itemAmmo(it.ammo), 1, it.q)) {
                P.inv.add(itemAmmo(it.ammo), 1, it.q);
                ftext(P.pos + V2(0, -24), std::string("+1 ") + AMMO[it.ammo].name, C_BLUE, 0.9f);
                audio.play(S_PICKUP, 0.4f);
                items.erase(items.begin() + i);
            } else
                i++;
            continue;
        }
        if (dist(P.pos, it.pos) < 13 && !P.inv.canAdd(itemWpn(it.wpn), 1)) notifyBlocked(itemWpn(it.wpn), 1);
        if (dist(P.pos, it.pos) < 13 && P.inv.canAdd(itemWpn(it.wpn), 1, it.q)) {
            P.inv.add(itemWpn(it.wpn), 1, it.q);
            ftext(P.pos + V2(0, -24), std::string("+") + WPN[it.wpn].name, C_BLUE, 1.0f);
            audio.play(S_PICKUP, 0.5f);
            items.erase(items.begin() + i);
        } else
            i++;
    }

    // --- gathering
    if (wantGather) {
        P.gathering = true;
        float need = ANIMAL[c->kind].gatherTime;
        P.gatherT += dt * TOOL_SPEED[P.up[U_TOOL]];
        P.gatherSfx -= dt;
        if (P.gatherSfx <= 0) {
            P.gatherSfx = 0.17f / TOOL_SPEED[P.up[U_TOOL]];
            puff(c->pos + V2(rng.range(-5, 5), -3), 1, HEX(0xd8c8a8));
        }
        if (P.gatherT >= need) {
            P.gatherT = 0;
            gatherStep(*c);
        }
    } else if (wantHarvest) {
        harvestStep(ht, dt);
    } else {
        P.gathering = false;
        P.gatherT = std::max(0.f, P.gatherT - dt * 2.f);
    }

    // --- shop / bandage / regen
    if (in.ePressed && !c && !structBusy && !ht.valid) {
        if (homesteadInteract()) {
        } else if (townInteract()) {
        } else if (delveInteract()) {
        } else if (interior < 0 && delve < 0 && nearQuestGiver()) {
            state = ST_QUEST;
            audio.play(S_CLICK);
        }
    }
    if (in.bandage && P.hp < P.maxhp) {  // Q: the lightest healing item equipped on the hotbar first, strong medicine last
        static const int order[] = {C_BANDAGE, C_SALVE, C_JERKY, C_COOKED, C_POULTICE, C_TONIC, C_STEW, C_MEDICINE};
        bool used = false;
        for (int cid : order)
            if (P.hotSlotOf(itemCons(cid)) >= 0 && P.inv.count(itemCons(cid)) > 0) {
                useConsumable(cid);
                used = true;
                break;
            }
        if (!used) {
            msg("NO HEALING EQUIPPED - EQUIP SOME FROM THE PACK (TAB)", C_RED);
            audio.play(S_ERROR);
        }
    }
    if (P.sinceHurt > 6.f) P.hp = std::min(P.maxhp, P.hp + (nearShop() ? 12.f : 1.6f) * dt);
}

// Pick the hunter's animation state from what the hunter is doing and advance it. Priority: death, a fresh hit, using an item,
// a throw, winding one up, a tool stroke / skinning, running, walking, idle.
void Game::updateHunterAnim(float dt, bool moving, bool sprinting, bool wantGather, bool wantHarvest, int swingTool, float swingFrac, int resCls) {
    Player& P = p;
    bool hit = false;
    if (P.sinceHurt < P.animSince) hit = true;  // sinceHurt was reset: a new hit
    P.animSince = P.sinceHurt;
    int st;
    bool restart = false;
    if (P.dead) st = HS_DEATH;
    else if (hit || (P.anim.state == HS_HURT && !P.anim.done)) { st = HS_HURT; restart = hit; }  // only a fresh hit starts it (not a respawn)
    else if (P.useT > 0) st = HS_USE;
    else if (P.throwAnim > 0) st = HS_THROW;
    else if (P.charging) st = HS_AIM;
    else if (wantHarvest) st = resCls == RCL_TREE ? HS_SWING : (resCls == RCL_ROCK || resCls == RCL_ORE) ? HS_MINE : HS_FORAGE;  // axe / pickaxe / hands on a plant
    else if (wantGather) st = HS_GATHER;  // kneeling at a carcass: skinning
    else if (moving) st = sprinting ? HS_RUN : HS_WALK;
    else st = HS_IDLE;
    (void)swingTool;
    P.impactT = std::max(0.f, P.impactT - dt);
    P.anim.play(st, &hunterClip(st), restart);
    P.anim.update(dt);
    if (isToolStroke(st)) P.anim.scrub(P.impactT > 0 ? 1.f : swingFrac * hunterSwingSpan(st));  // the recovery right after a hit, else follow the swing
    else if (st == HS_AIM) P.anim.scrub(clampf(P.charge, 0.f, 1.f));
}

void Game::updateProjectiles(float dt) {
    for (size_t i = 0; i < projs.size();) {
        Proj& pr = projs[i];
        bool done = false;
        float step = pr.speed * dt;
        int sub = std::max(1, (int)ceilf(step / 3.f));
        float s = step / sub;
        for (int k = 0; k < sub && !done; k++) {
            pr.last = pr.pos;
            pr.pos += pr.dir * s;
            pr.travelled += s;
            for (auto& a : animals) {
                if (a.dead) continue;
                V2 c = a.pos + V2(0, -a.bodyH - a.air);
                if (dist(pr.pos, c) < ANIMAL[a.kind].hitR * a.scale + 3) {
                    hitAnimal(a, pr.dmg, pr.dir, pr.crit);
                    if (rng.chance((pr.ammo >= 0 ? AMMO[pr.ammo].lost : WPN[pr.wpn].brk) * (pr.ammo >= 0 && p.has(CG_QUIVER) ? 0.5f : 1.f) * (pr.ammo >= 0 ? 1.f - skillBonus(SK_RANGED) : 1.f))) {
                        for (int q = 0; q < 6; q++)
                            parts.push_back({pr.pos.x, pr.pos.y, 4, rng.range(-50, 50), rng.range(-30, 30), rng.range(30, 80), 0.5f, 0.5f, HEX(0xd8dce4), 1, true});
                        audio.play(S_BREAK, 0.6f);
                        ftext(a.pos + V2(0, -a.bodyH - 22), "SNAP", C_DIM, 0.7f);
                    } else {
                        items.push_back({a.pos + V2(rng.range(-9, 9), rng.range(0, 7)), pr.wpn, rng.range(0, 6.28f), 0, pr.ammo}); items.back().q = pr.ammo >= 0 ? pr.aq : pr.q;
                    }
                    done = true;
                    break;
                }
            }
            if (done) break;
            for (auto& m : mons) {  // the creatures of a delve
                if (m.state == MN_DEAD) continue;
                const MonsterDef& MD = MONDEF(m.kind);
                const V2 c = m.pos + V2(0, -MD.bodyH - ((MD.flags & MF_FLY) ? 12.f : 0.f));
                if (dist(pr.pos, c) < MD.hitR + 3) {
                    const bool landed = hitMonster(m, pr.dmg, pr.dir, pr.crit, pr.wpn);
                    if (landed && rng.chance((pr.ammo >= 0 ? AMMO[pr.ammo].lost : WPN[pr.wpn].brk) * (pr.ammo >= 0 && p.has(CG_QUIVER) ? 0.5f : 1.f) * (pr.ammo >= 0 ? 1.f - skillBonus(SK_RANGED) : 1.f))) {
                        for (int q = 0; q < 6; q++)
                            parts.push_back({pr.pos.x, pr.pos.y, 4, rng.range(-50, 50), rng.range(-30, 30), rng.range(30, 80), 0.5f, 0.5f, HEX(0xd8dce4), 1, true});
                        audio.play(S_BREAK, 0.6f);
                    } else {
                        items.push_back({m.pos + V2(rng.range(-9, 9), rng.range(0, 7)), pr.wpn, rng.range(0, 6.28f), 0, pr.ammo}); items.back().q = pr.ammo >= 0 ? pr.aq : pr.q;
                    }
                    done = true;
                    break;
                }
            }
            if (done) break;
            V2 g = pr.pos + V2(0, 9);
            int tx = (int)floorf(g.x / TS), ty = (int)floorf(g.y / TS);
            bool obstacle = tx < 0 || ty < 0 || tx >= MW || ty >= MH || world.solid[ty][tx] || world.tile[ty][tx] == T_CLIFF || (delve >= 0 && world.solidTile(tx, ty));
            if (obstacle) {
                V2 lg = pr.last + V2(0, 9);
                items.push_back({lg, pr.wpn, pr.angle + pr.spin * (pr.travelled / pr.speed), 0, pr.ammo}); items.back().q = pr.ammo >= 0 ? pr.aq : pr.q;
                for (int q = 0; q < 4; q++)
                    parts.push_back({pr.pos.x, pr.pos.y, 3, rng.range(-40, 40), rng.range(-20, 20), rng.range(20, 60), 0.4f, 0.4f, HEX(0xffe9a8), 1, true});
                audio.play(S_CLICK, 0.5f);
                done = true;
            } else if (pr.travelled >= pr.range) {
                if (world.water(tx, ty)) {
                    for (int q = 0; q < 8; q++)
                        parts.push_back({g.x, g.y, 1, rng.range(-30, 30), rng.range(-15, 15), rng.range(30, 70), 0.6f, 0.6f, HEX(0xcfe9f5), 1, true});
                    ftext(g + V2(0, -8), "SPLASH", C_BLUE, 0.8f);
                } else {
                    float ang = pr.angle + pr.spin * (pr.travelled / pr.speed);
                    items.push_back({g, pr.wpn, ang, 0, pr.ammo}); items.back().q = pr.ammo >= 0 ? pr.aq : pr.q;
                    puff(g, 3, HEX(0xcdb98f));
                }
                done = true;
            }
        }
        if (done) projs.erase(projs.begin() + i);
        else i++;
    }
}
