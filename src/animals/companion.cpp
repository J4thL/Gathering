// companion.cpp - a raised, grown mount can be made the hunter's COMPANION (a button in the homestead's animal panel).
//
// Away from the homestead it walks along behind the hunter (a light follower, not a wild animal: nothing hunts it, nothing shoots it) and
// earns BOND by being near the kills. Each bond level adds weapon damage and makes it dig up small finds on the way.
// It is still the same animal in the homestead's list: it is flagged `away` while with the hunter and goes home when the hunter does.
#include "core/game.hpp"

HAnimal* Game::companionAnimal() {
    for (auto& a : hs.animals)
        if (a.companion && a.kind == DA_MOUNT && a.adult(DOMS[a.kind])) return &a;
    return nullptr;
}

int Game::bondLevel(int bond) {
    int l = 0;
    for (int i = 1; i < COMPANION_LEVELS; i++) if (bond >= COMPANION_BOND_AT[i]) l = i;
    return l;
}

// weapon damage the companion adds to every throw while it is with the hunter
float Game::companionDamage() const {
    if (!companion.active) return 0.f;
    for (auto& a : hs.animals)
        if (a.companion && a.away) return COMPANION_DAMAGE_PER_LEVEL * (1 + bondLevel(a.bond));
    return 0.f;
}

void Game::companionHome() {  // back to the pen: the companion is no longer with the hunter
    for (auto& a : hs.animals)
        if (a.away) {
            a.away = false;
            a.px = a.tx = p.pos.x + 14.f;
            a.py = a.ty = p.pos.y + 6.f;
        }
    companion.active = false;
}

// A kill by the hunter with the companion close by: the bond grows.
void Game::companionKill() {
    if (!companion.active) return;
    for (auto& a : hs.animals)
        if (a.companion && a.away && dist(companion.pos, p.pos) < 220.f) {
            const int before = bondLevel(a.bond);
            a.bond++;
            if (bondLevel(a.bond) > before) {
                msg(std::string("YOUR ") + ANIMAL[a.species].name + " BONDS WITH YOU: LEVEL " + S(bondLevel(a.bond) + 1) + " - +" + S((int)lroundf(COMPANION_DAMAGE_PER_LEVEL * 100)) + "% DAMAGE", C_GOLD);
                audio.play(S_CRAFT);
            }
        }
}

void Game::updateCompanion(float dt, bool forceWilds) {
    HAnimal* a = companionAnimal();
    const bool wilds = forceWilds || (!inHomestead() && interior < 0 && delve < 0 && world.mapId < B_COUNT);
    if (!a || p.dead || riddenMount()) {  // none chosen, or the hunter is down or in the saddle
        if (companion.active) companionHome();
        return;
    }
    if (!wilds) {
        if (inHomestead() && a->away) companionHome();  // home again
        companion.active = false;                       // (indoors and in caves it waits outside)
        return;
    }
    if (!companion.active) {  // setting out: it joins the hunter
        companion.active = true;
        companion.pos = p.pos + V2(-14, 8);
        companion.forageT = 40.f;
        a->away = true;
    }
    V2& pos = companion.pos;
    const V2 behind = p.pos - p.aim * 20.f;
    const float d = dist(pos, behind);
    if (dist(pos, p.pos) > 300.f) pos = behind;  // fell behind (a portal, a long run): it catches up
    companion.moving = false;
    if (d > 6.f) {
        const V2 dir = (behind - pos) * (1.f / d);
        const float sp = std::min(d * 4.f, 125.f);
        const V2 np = pos + dir * sp * dt;
        if (!world.blocked(np.x, np.y, 4.f)) pos = np;
        else if (!world.blocked(np.x, pos.y, 4.f)) pos.x = np.x;
        else if (!world.blocked(pos.x, np.y, 4.f)) pos.y = np.y;
        companion.face8 = facingFromDir(dir.x, dir.y, companion.face8);
        companion.moving = sp > 12.f;
    }
    companion.animT += dt;
    // small finds: dug up now and then along the way (more often with a deeper bond)
    companion.forageT -= dt;
    if (companion.forageT <= 0) {
        const int lvl = bondLevel(a->bond);
        companion.forageT = 150.f / (1 + lvl) * rng.range(0.8f, 1.2f);
        static const int FINDS[5] = {M_HERB, M_BERRY, M_MUSHROOM, M_FIBER, M_BRANCH};
        const int mat = lvl >= 3 && rng.chance(0.25f) ? (int)M_AMBER : FINDS[rng.irange(0, 4)];
        GroundItem g{};
        g.pos = pos + V2(rng.range(-8, 8), 6);
        g.item = itemMat(mat);
        g.n = 1;
        g.lock = 0.f;
        g.wpn = 0;
        g.ammo = -1;
        items.push_back(g);
        ftext(pos + V2(0, -16), "FOUND " + std::string(MAT_NAME[mat]), C_GREEN, 1.4f);
        puff(pos, 3, HEX(0xcdb98f));
    }
}

void Game::drawCompanion() {
    if (!companion.active) return;
    HAnimal* a = nullptr;
    for (auto& h : hs.animals) if (h.companion && h.away) a = &h;
    if (!a) return;
    HAnimal tmp = *a;
    tmp.away = false;
    tmp.ridden = false;
    tmp.px = companion.pos.x;
    tmp.py = companion.pos.y;
    tmp.face8 = companion.face8;
    tmp.state = companion.moving ? 1 : 0;
    tmp.animT = companion.animT;
    drawHsAnimal(tmp);
}
