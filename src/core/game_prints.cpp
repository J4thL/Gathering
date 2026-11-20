// game_prints.cpp - footprints: what walking leaves behind.
// A small pool of prints (FOOT_POOL), shared by the hunter and every animal, where each walker keeps at most FOOT_PER_WALKER of its own,
// newest first: stamping an eleventh print takes over the walker's oldest. So a trail is always the last ten steps, whoever made it, and
// the cost is fixed: the pool never grows and a frame draws at most FOOT_POOL tiny marks. Prints are only drawn, never saved.
// What a print looks like depends on the ground it was made on: dark marks in bank, mud and dirt, lighter in sand, blue-grey in snow, an
// expanding ripple in shallow and mid water, and a very faint mark on grass and the forest floors.
#include "core/game.hpp"

namespace {
constexpr float FOOT_STRIDE = 12.f;  // pixels between two of the hunter's prints
enum FpKind : uint8_t { FP_NONE, FP_SOFT, FP_SAND, FP_SNOW, FP_WATER, FP_FAINT };

uint8_t fpKindOf(uint8_t t) {
    switch (t) {
        case T_BANK: case T_MUD: case T_MURK: case T_DIRT: return FP_SOFT;
        case T_SAND: case T_DUNE: return FP_SAND;
        case T_SNOW: case T_SNOWP: return FP_SNOW;
        case T_SHALLOW: case T_LAKE_SHALLOW: case T_LAKE_MID: return FP_WATER;
        case T_GRASS: case T_DARK: case T_LEAF: case T_NEEDLES: case T_MOSS: case T_ROOTS: return FP_FAINT;
        default: return FP_NONE;  // stone, floors, ice, cliffs: nothing to mark
    }
}
}  // namespace

int Game::footprintsActive(int owner) const {
    int n = 0;
    for (const Footprint& f : prints)
        if (f.age < FOOT_LIFE && (owner < 0 || f.owner == owner)) n++;
    return n;
}

void Game::addFootprint(float x, float y, int owner, float dx, float dy, bool left) {
    if (inHomestead() || world.isInterior() || delve >= 0) return;
    const uint8_t kind = fpKindOf(world.tileAt(x, y));
    if (kind == FP_NONE) return;
    // a slot: the walker's own oldest print when it already has FOOT_PER_WALKER, else a free or the oldest slot of the pool
    int own = 0, ownOldest = -1, freeSlot = -1, oldest = 0;
    for (int i = 0; i < FOOT_POOL; i++) {
        const Footprint& f = prints[i];
        if (f.age >= FOOT_LIFE) { if (freeSlot < 0) freeSlot = i; continue; }
        if (f.age > prints[oldest].age) oldest = i;
        if (f.owner == owner) { own++; if (ownOldest < 0 || f.age > prints[ownOldest].age) ownOldest = i; }
    }
    const int slot = own >= FOOT_PER_WALKER ? ownOldest : (freeSlot >= 0 ? freeSlot : oldest);
    Footprint& f = prints[slot];
    const float l = std::sqrt(dx * dx + dy * dy);
    if (l > 0.001f) { dx /= l; dy /= l; } else { dx = 1.f; dy = 0.f; }
    const float side = left ? -2.f : 2.f;  // the two feet fall either side of the line of travel
    f.x = x - dy * side;
    f.y = y + dx * side * 0.6f;
    f.dx = (int8_t)(std::fabs(dx) >= std::fabs(dy) ? (dx >= 0 ? 1 : -1) : 0);
    f.dy = (int8_t)(std::fabs(dx) < std::fabs(dy) ? (dy >= 0 ? 1 : -1) : 0);
    f.age = 0.f;
    f.owner = owner;
    f.kind = kind;
}

void Game::updateFootprints(float dt) {
    for (Footprint& f : prints)
        if (f.age < FOOT_LIFE) f.age += dt;
    // the hunter: a print every FOOT_STRIDE pixels walked, left foot then right (so the trail follows the real steps, at any speed)
    if (state == ST_PLAY && !p.dead) {
        const float d = dist(p.pos, fpPrevP);
        const V2 dir = p.pos - fpPrevP;
        if (!fpInitP || d > 24.f) { fpInitP = true; fpAccP = 0.f; }
        else {
            fpAccP += d;
            if (fpAccP >= FOOT_STRIDE && d > 0.05f) {
                fpAccP = 0.f;
                fpLP = !fpLP;
                addFootprint(p.pos.x, p.pos.y, 0, dir.x, dir.y, fpLP);
            }
        }
        fpPrevP = p.pos;
    } else fpInitP = false;
    // the animals near the screen leave prints of their own, one every few pixels they walk
    const float cx = camX + LW * 0.5f, cy = camY + LH * 0.5f;
    for (size_t i = 0; i < animals.size(); i++) {
        Animal& a = animals[i];
        if (a.dead || a.gone || a.air > 2.f) { a.fpInit = false; continue; }
        if (std::fabs(a.pos.x - cx) > LW * 0.8f || std::fabs(a.pos.y - cy) > LH * 0.8f) { a.fpInit = false; continue; }
        if (!a.fpInit) { a.fpInit = true; a.fpPrev = a.pos; a.fpAcc = 0.f; continue; }
        const float d = dist(a.pos, a.fpPrev);
        const V2 dir = a.pos - a.fpPrev;
        a.fpPrev = a.pos;
        if (d > 24.f) { a.fpAcc = 0.f; continue; }  // it was moved, not walking
        a.fpAcc += d;
        if (a.fpAcc >= 9.f && d > 0.05f) {
            a.fpAcc = 0.f;
            a.fpL = !a.fpL;
            addFootprint(a.pos.x, a.pos.y, (int)i + 1, dir.x, dir.y, a.fpL);
        }
    }
}

void Game::drawFootprints() {
    for (const Footprint& f : prints) {
        if (f.age >= FOOT_LIFE) continue;
        const float k = 1.f - f.age / FOOT_LIFE;  // 1 fresh .. 0 gone
        const int sx = (int)std::lround(f.x - camX), sy = (int)std::lround(f.y - camY);
        if (sx < -8 || sy < -8 || sx > LW + 8 || sy > LH + 8) continue;
        const bool walkerIsAnimal = f.owner > 0;
        const bool horiz = f.dx != 0;
        auto mark = [&](u32 r, u32 g, u32 b, float alpha) {  // a print: a short stroke along the travel, smaller for an animal's paw
            const int a = std::max(1, (int)(alpha * k * 255.f));
            const int w = walkerIsAnimal ? 2 : (horiz ? 4 : 2), h = walkerIsAnimal ? 2 : (horiz ? 2 : 4);
            fillRect(sx, sy, w, h, RGBA(r, g, b, a));
        };
        switch (f.kind) {
            case FP_SOFT:  // pressed earth: a dark dent with a thin light rim on the side the light comes from
                fillRect(sx - 1, sy - 1, walkerIsAnimal ? 3 : (horiz ? 5 : 3), 1, RGBA(190, 148, 100, (int)(0.5f * k * 255.f)));
                mark(22, 12, 4, 0.9f);
                break;
            case FP_SAND: mark(118, 92, 52, 0.55f); break;
            case FP_SNOW: mark(112, 130, 172, 0.65f); break;
            case FP_FAINT: {  // grass and forest floors: a faint pressed mark, a few percent dark
                const int a = std::max(1, (int)(0.16f * k * 255.f));
                fillRect(sx, sy, horiz ? 2 : 1, horiz ? 1 : 2, RGBA(18, 28, 12, a));
                break;
            }
            case FP_WATER: {  // a ripple that spreads and thins out
                const float r = 1.f + 5.f * (1.f - k);
                const int a = std::max(1, (int)(0.7f * k * 255.f));
                for (int s = 0; s < 12; s++) {
                    const float ang = s * 0.5236f;
                    fillRect(sx + (int)std::lround(r * std::cos(ang)), sy + (int)std::lround(r * 0.55f * std::sin(ang)), 1, 1, RGBA(232, 248, 255, a));
                }
                break;
            }
            default: break;
        }
    }
}
