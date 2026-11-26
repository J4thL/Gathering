// mount_ride.cpp - riding a raised mount: V saddles up / dismounts. Speed and carry weight come from mounts.hpp; the mount
// stays in the homestead's animal list (flagged `ridden`) so it is saved and grows with everyone else.
#include "core/game.hpp"

HAnimal* Game::riddenMount() {
    for (auto& a : hs.animals)
        if (a.ridden) return &a;
    return nullptr;
}

HAnimal* Game::saddledMount() {
    for (auto& a : hs.animals)
        if (a.saddled && a.kind == DA_MOUNT && a.adult(DOMS[a.kind])) return &a;
    return nullptr;
}

// the carrying weight a mount adds right now: its own, plus half again with saddlebags in the pack
int Game::mountKgNow(const HAnimal& m) const {
    return (int)lroundf(mountCarry(m.species, m.tier) * (p.has(CG_SADDLEBAG) ? 1.5f : 1.f));
}

// The parked mount goes back to its pen (the hunter wandered off, left the outdoors, or it was needed elsewhere).
void Game::parkedHome(bool say) {
    if (!parked.active) return;
    for (auto& a : hs.animals)
        if (a.away && a.saddled) {
            a.away = false;
            a.px = a.tx = (HS_PORTAL_X + 0.5f) * TS;
            a.py = a.ty = (HS_PORTAL_Y + 3.f) * TS;
        }
    parked.active = false;
    if (say) msg("YOUR MOUNT WENT HOME - PRESS V TO SUMMON IT", C_DIM);
}

void Game::dismount(bool quiet) {
    HAnimal* m = riddenMount();
    if (!m) return;
    m->ridden = false;
    m->px = m->tx = p.pos.x;
    m->py = m->ty = p.pos.y;
    const bool leave = inWilds() && !quiet && !p.dead;
    if (leave) {  // it waits right here, idle, until the hunter strays too far
        m->away = true;
        parked.active = true;
        parked.pos = p.pos + V2(0, 4);
        parked.face8 = p.face8;
        parked.animT = 0;
        parked.map = world.mapId;
    } else if (!inHomestead()) {  // no way to wait here (a building, a cave, a death): it finds its own way back to its pen
        m->px = m->tx = (HS_PORTAL_X + 0.5f) * TS;
        m->py = m->ty = (HS_PORTAL_Y + 3.f) * TS;
    }
    p.mountKg = 0;
    p.syncPack();
    mountAnimT = 0;
    if (!quiet) msg(inHomestead() ? "YOU DISMOUNT" : "YOU DISMOUNT - THE MOUNT RETURNS TO THE HOMESTEAD", C_DIM);
}

bool Game::toggleMount() {
    if (p.dead || state != ST_PLAY) return false;
    if (riddenMount()) {
        dismount();
        audio.play(S_CLICK);
        return true;
    }
    HAnimal* m = saddledMount();
    if (!m) { msg("NO SADDLED MOUNT - RAISE A BABY AT THE HOMESTEAD", C_DIM); return false; }
    if (world.mapId >= B_COUNT || interior >= 0 || delve >= 0) { msg("YOUR MOUNT WAITS OUTSIDE", C_RED); audio.play(S_ERROR); return false; }
    const bool summon = !inHomestead();  // outdoors the mount can always be called: it waits where it was left, or comes from its pen
    if (!summon && (m->px - p.pos.x) * (m->px - p.pos.x) + (m->py - p.pos.y) * (m->py - p.pos.y) > 70.f * 70.f) { msg("YOUR MOUNT IS TOO FAR AWAY", C_DIM); return false; }
    if (companion.active) companionHome();  // a mount is either ridden or a companion, never both
    for (auto& o : hs.animals) o.companion = false;
    bool called = false;
    if (summon) {
        if (parked.active && dist(parked.pos, p.pos) < 40.f) {
            // right next to it: just climb on
        } else {
            called = true;
            parked.active = false;
        }
        m->px = m->tx = p.pos.x;
        m->py = m->ty = p.pos.y;
        if (called) puff(p.pos, 6, HEX(0xcdb98f));
    }
    parked.active = false;
    m->away = false;
    m->ridden = true;
    p.mountKg = mountKgNow(*m);
    p.syncPack();
    msg("YOU MOUNT THE " + std::string(ANIMAL[m->species].name) + " +" + S(p.mountKg) + " KG", C_GREEN);
    audio.play(S_CLICK);
    return true;
}

void Game::updateParked(float dt) {
    if (!parked.active) return;
    bool saddledAway = false;
    for (auto& a : hs.animals) if (a.away && a.saddled) saddledAway = true;
    if (!saddledAway) { parked.active = false; return; }  // (unsaddled or removed while it waited)
    if (!inWilds() || world.mapId != parked.map) { parkedHome(false); return; }
    if (dist(parked.pos, p.pos) > PARK_RANGE) parkedHome(true);
    parked.animT += dt;
}

void Game::drawParked() {
    if (!parked.active) return;
    for (auto& h : hs.animals)
        if (h.away && h.saddled) {
            HAnimal tmp = h;
            tmp.away = false;
            tmp.ridden = false;
            tmp.px = parked.pos.x;
            tmp.py = parked.pos.y;
            tmp.face8 = parked.face8;
            tmp.state = 0;
            tmp.animT = parked.animT;
            drawHsAnimal(tmp);
            return;
        }
}

void Game::updateMount(float dt, bool moving) {
    HAnimal* m = riddenMount();
    if (!m) return;
    if (world.mapId >= B_COUNT || p.dead) {  // no buildings, caves or mines on a mount
        dismount(true);
        msg("YOUR MOUNT WAITS OUTSIDE", C_DIM);
        return;
    }
    m->px = m->tx = p.pos.x;
    m->py = m->ty = p.pos.y;
    m->face8 = p.face8;
    const int kg = mountKgNow(*m);  // saddlebags picked up (or dropped) while riding
    if (kg != p.mountKg) { p.mountKg = kg; p.syncPack(); }
    mountMoving = moving;
    mountAnimT += dt * (moving ? 1.f : 0.f);
}
