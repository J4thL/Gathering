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

const char* const COMPANION_ORDER_NAME[3] = {"FOLLOWS YOU", "FETCHES YOUR THROWN WEAPONS", "SCOUTS FOR GAME"};

// K_COMMAND: cycle the companion's order (scouting needs bond level 2: 25 kills made together)
void Game::companionOrder() {
    HAnimal* a = companionAnimal();
    if (!a || !companion.active) {
        msg(a ? "YOUR COMPANION IS AT HOME - ORDERS WORK OUTSIDE" : "NO COMPANION - CHOOSE ONE IN THE HOMESTEAD ANIMAL LIST", C_DIM);
        return;
    }
    int m = companion.mode;
    do m = (m + 1) % CM_COUNT; while (m == CM_SCOUT && bondLevel(a->bond) < 1);
    companion.mode = m;
    companion.fetching = false;
    companion.orderT = 0;
    msg(std::string("YOUR ") + ANIMAL[a->species].name + " " + COMPANION_ORDER_NAME[m], C_GOLD);
    audio.play(S_CLICK);
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
    if (dist(pos, p.pos) > 300.f) { pos = behind; companion.fetching = false; }  // fell behind (a portal, a long run): it catches up
    // ---- orders
    V2 goal = behind;
    companion.orderT -= dt;
    if (companion.mode == CM_FETCH) {
        if (!companion.fetching && companion.orderT <= 0) {  // look for a thrown weapon or arrow lying a way off
            companion.orderT = 0.6f;
            float best = 1e9f;
            for (auto& it : items) {
                if (it.item >= 0) continue;  // (stacks you dropped are left alone)
                const float dp = dist(it.pos, p.pos);
                if (dp < 26.f || dp > 230.f || dist(it.pos, pos) > 260.f) continue;
                if (dp < best) { best = dp; companion.fetchPos = it.pos; companion.fetching = true; }
            }
        }
        if (companion.fetching) {
            GroundItem* tgt = nullptr;
            for (auto& it : items)
                if (it.item < 0 && dist(it.pos, companion.fetchPos) < 6.f) tgt = &it;
            if (!tgt) companion.fetching = false;  // you picked it up yourself
            else if (dist(pos, tgt->pos) < 9.f) {  // got it: it drops it at your feet
                tgt->pos = p.pos + V2(rng.range(-6, 6), 8);
                tgt->lock = 0.f;
                companion.fetching = false;
                companion.orderT = 0.8f;
                ftext(pos + V2(0, -16), "FETCHED!", C_GREEN, 1.f);
            } else goal = tgt->pos;
        }
    } else if (companion.mode == CM_SCOUT && companion.orderT <= 0) {  // sniff out the animals around (wider with a deeper bond)
        companion.orderT = 7.f;
        const float R = 170.f + 45.f * bondLevel(a->bond);
        int n = 0;
        for (auto& an : animals)
            if (!an.dead && !an.gone && dist(an.pos, p.pos) < R) { an.scoutT = 9.f; n++; }
        ftext(pos + V2(0, -16), n ? "SNIFFS OUT " + S(n) : std::string("NOTHING NEAR"), n ? C_GOLD : C_DIM, 1.2f);
    }
    const float d = dist(pos, goal);
    companion.moving = false;
    if (d > (companion.fetching ? 2.f : 6.f)) {
        const V2 dir = (goal - pos) * (1.f / d);
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
