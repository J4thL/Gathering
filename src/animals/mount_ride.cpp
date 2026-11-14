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

void Game::dismount(bool quiet) {
    HAnimal* m = riddenMount();
    if (!m) return;
    m->ridden = false;
    m->px = m->tx = p.pos.x;
    m->py = m->ty = p.pos.y;
    if (!inHomestead()) {  // left behind in the wild: it finds its own way back to its pen
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
    if (world.mapId >= B_COUNT) { msg("YOUR MOUNT WAITS OUTSIDE", C_RED); audio.play(S_ERROR); return false; }
    if (!inHomestead()) { msg("YOUR MOUNT IS AT THE HOMESTEAD", C_DIM); return false; }
    if ((m->px - p.pos.x) * (m->px - p.pos.x) + (m->py - p.pos.y) * (m->py - p.pos.y) > 70.f * 70.f) { msg("YOUR MOUNT IS TOO FAR AWAY", C_DIM); return false; }
    m->ridden = true;
    p.mountKg = mountCarry(m->species, m->tier);
    p.syncPack();
    msg("YOU MOUNT THE " + std::string(ANIMAL[m->species].name) + " +" + S(p.mountKg) + " KG", C_GREEN);
    audio.play(S_CLICK);
    return true;
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
    mountMoving = moving;
    mountAnimT += dt * (moving ? 1.f : 0.f);
}
