// melee.cpp - the swing: input (tap = quick, hold = heavy), the three-hit combo, hit detection against animals and dungeon monsters, visuals.
#include "core/game.hpp"
#include "core/logic_util.hpp"

// the weapon swung: the selected one when the hunter owns it and has it equipped, else bare fists (-1)
int Game::meleeWeapon() const {
    const int w = p.sel;
    if (w >= 0 && w < W_COUNT && p.equippedWpn(w) && p.wpn(w) > 0) return w;
    return -1;
}

bool Game::startMelee(bool heavy) {
    if (p.dead || p.meleeT > 0 || p.cd > 0 || p.rollT > 0 || p.charging || p.hand >= 0 || riddenMount() || p.gathering) return false;
    const int w = meleeWeapon();
    MeleeDef md = meleeDef(w);
    float cost = md.stamina;
    float dur = md.dur;
    if (heavy) { cost = MELEE_HEAVY_STAMINA; dur = MELEE_HEAVY_DUR; }
    if (p.exhausted || p.stamina < cost) {
        if (noAmmoT <= 0) { ftext(p.pos + V2(0, -24), "TOO TIRED", C_RED, 1.0f); noAmmoT = 0.8f; }
        return false;
    }
    p.stamina -= cost;
    p.staDelay = 0.8f;
    // the combo: a swing soon after the last one steps it on; the third is the finisher
    p.meleeStep = (p.meleeComboT > 0 && !heavy) ? (p.meleeStep + 1) % 3 : 0;
    p.meleeHeavy = heavy;
    p.meleeT = p.meleeDur = dur;
    p.meleeStruck = false;
    p.meleeAim = p.aim;
    p.cd = dur;
    p.kb += p.aim * (heavy ? 95.f : 55.f);  // a step into the swing
    audio.play(heavy ? S_THROW : S_THROW, heavy ? 0.7f : 0.45f);
    return true;
}

// the blow lands: everything in the arc takes damage (up to MELEE_MAX_TARGETS)
void Game::meleeStrike() {
    const int w = meleeWeapon();
    const MeleeDef md = meleeDef(w);
    const bool heavy = p.meleeHeavy, fin = !heavy && p.meleeStep == 2;
    const float reach = md.reach + (heavy ? MELEE_HEAVY_REACH : 0.f);
    const float cosArc = cosf((md.arcDeg * (heavy ? MELEE_HEAVY_ARC : 1.f) * 0.5f) * 3.14159265f / 180.f);
    const V2 hand = p.pos + V2(0, -8);
    const V2 dir = p.meleeAim;
    const float base = w < 0 ? MELEE_FIST_DMG : WPN[w].dmg * md.dmgMul;
    float mul = 1.f + skillBonus(SK_HUNTING) + companionDamage();
    if (heavy) mul *= MELEE_HEAVY_MUL;
    if (fin) mul *= MELEE_FINISHER_MUL;
    if (w >= 0) mul *= qualityDef(p.inv.bestQuality(itemWpn(w))).stat;
    if (p.has(CG_CROWN)) mul *= 1.15f;
    int hits = 0;
    auto inArc = [&](V2 target, float hitR) {
        const V2 d = target - hand;
        const float dl = d.len();
        if (dl > reach + hitR) return false;
        if (dl < 1.f) return true;
        return (d.x * dir.x + d.y * dir.y) / dl >= cosArc || dl < hitR + 6.f;
    };
    for (auto& a : animals) {
        if (hits >= MELEE_MAX_TARGETS) break;
        if (a.dead || a.gone || a.air > 4.f) continue;
        const V2 c = a.pos + V2(0, -a.bodyH);
        if (!inArc(c, ANIMAL[a.kind].hitR * a.scale)) continue;
        const bool crit = rng.chance(heavy ? 0.25f : 0.10f);
        const float dmg = base * mul * rng.range(0.92f, 1.08f) * (crit ? 1.6f : 1.f);
        hitAnimal(a, dmg, dir, crit, (w >= 0 && !isLauncher(w)) ? w : -1);
        if (heavy && a.hp > 0) { a.stunT = std::max(a.stunT, a.tier >= TR_LEGENDARY ? 0.5f : 1.0f); a.kb += dir * 90.f; }  // a heavy blow rocks it
        hits++;
    }
    if (delve >= 0)
        for (auto& m : mons) {
            if (hits >= MELEE_MAX_TARGETS) break;
            if (m.state == MN_DEAD) continue;
            if (!inArc(m.pos + V2(0, -MONDEF(m.kind).bodyH), MONDEF(m.kind).hitR)) continue;
            const bool crit = rng.chance(heavy ? 0.25f : 0.10f);
            const float dmg = base * mul * rng.range(0.92f, 1.08f) * (crit ? 1.6f : 1.f);
            hitMonster(m, dmg, dir, crit, w);
            hits++;
        }
    if (hits) {
        shakeAmt = std::max(shakeAmt, heavy || fin ? 3.f : 1.5f);
        if (fin) ftext(p.pos + V2(0, -30), "COMBO!", C_GOLD, 0.9f);
        if (w >= 0 && !isLauncher(w)) grantXp(SK_HUNTING, heavy ? 2 : 1);
    }
}

void Game::updateMelee(float dt) {
    // input: tap the button for a quick swing, hold it for a heavy one (decided when it is let go)
    const bool held = in.melee && state == ST_PLAY && !p.dead;
    if (held && !p.meleePrev) p.meleeHold = 0;
    if (held) p.meleeHold += dt;
    if (!held && p.meleePrev && p.meleeHold > 0) {
        startMelee(p.meleeHold >= MELEE_HEAVY_HOLD);
        p.meleeHold = 0;
    }
    p.meleePrev = held;
    if (p.meleeComboT > 0) p.meleeComboT -= dt;
    if (p.meleeT > 0) {
        p.meleeT = std::max(0.f, p.meleeT - dt);
        if (!p.meleeStruck && p.meleeT <= p.meleeDur * 0.55f) {  // the blow lands after the wind-up
            p.meleeStruck = true;
            meleeStrike();
        }
        if (p.meleeT <= 0) p.meleeComboT = MELEE_COMBO_WINDOW;
    }
}

// the weapon sweeping across the swing arc, with a faint trail
void Game::drawMelee() {
    if (p.dead) return;
    const bool charging = p.meleePrev && p.meleeHold >= MELEE_HEAVY_HOLD && p.meleeT <= 0;
    if (p.meleeT <= 0 && !charging) return;
    const int w = meleeWeapon();
    const MeleeDef md = meleeDef(w);
    const bool heavy = p.meleeHeavy;
    const V2 aim = charging ? p.aim : p.meleeAim;
    const float baseAng = atan2f(aim.y, aim.x);
    const float arc = md.arcDeg * (heavy || charging ? MELEE_HEAVY_ARC : 1.f) * 3.14159265f / 180.f;
    const float f = charging ? 0.f : 1.f - p.meleeT / p.meleeDur;  // 0..1 through the swing
    const float sweep = charging ? -0.5f : -0.5f + std::min(1.f, f / 0.8f);  // wind back, then across
    const float ang = baseAng + sweep * arc;
    const V2 hand = p.pos + V2(0, -8);
    const V2 tip = hand + V2(cosf(ang), sinf(ang)) * (md.reach * 0.55f);
    if (w >= 0) sprW(A.wpnWorld[w], tip.x, tip.y, false, 1.f, ang * 57.2958, true);
    if (!charging) {  // trail of pips behind the blade
        for (int i = 1; i <= 5; i++) {
            const float a2 = ang - (i * 0.12f) * (arc > 0 ? 1.f : 0.f);
            const V2 q = hand + V2(cosf(a2), sinf(a2)) * md.reach;
            fillRect((int)q.x - camX, (int)q.y - camY, 2, 2, RGBA(240, 240, 255, 150 - i * 25));
        }
    } else {  // the heavy swing is ready: a pulsing ring at the hunter's feet
        const int al = 90 + (int)(80 * sinf(timeAcc * 18.f));
        for (int i = 0; i < 12; i++) {
            const float t = i * 6.2832f / 12.f;
            fillRect((int)(p.pos.x + cosf(t) * 14.f) - camX, (int)(p.pos.y + sinf(t) * 9.f) - camY, 2, 1, RGBA(255, 210, 90, al));
        }
    }
}
