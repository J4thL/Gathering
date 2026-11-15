// grass_react.cpp - the grass answers to whoever walks through it: tufts, flowers, reeds and the like bend away from a moving hunter or
// animal and then rustle back to rest. Only the little record of the last kick is kept (when, how hard, which way); the renderer turns
// it into a lean and a wobble. Nothing here touches gameplay state or the gameplay random stream.
#include "core/game.hpp"

void Game::updateGrass(float dt) {
    const size_t n = world.decor.size();
    if (grassMap != world.mapId || grassT.size() != n) {  // another map: start with calm grass
        grassMap = world.mapId;
        grassT.assign(n, -100.f);
        grassAmp.assign(n, 0.f);
        grassDir.assign(n, 0);
        grassPrev = p.pos;
        grassPuffCd = 0.f;
    }
    grassPuffCd = std::max(0.f, grassPuffCd - dt);
    if (n == 0 || dt <= 0.f) return;
    auto reactTo = [&](size_t i, float dx, float amp) {
        const float left = grassAmp[i] * std::max(0.f, 1.f - (timeAcc - grassT[i]) / GRASS_REST);
        if (amp <= left && grassT[i] > timeAcc - GRASS_REST * 0.5f) return false;  // still swaying harder than this would make it
        grassT[i] = timeAcc;
        grassAmp[i] = amp;
        grassDir[i] = dx >= 0.f ? 1 : -1;
        return true;
    };
    struct Mover { V2 pos; float speed, reach; bool hunter; };
    Mover ms[40];
    int nm = 0;
    if (!p.dead && interior < 0) {
        const float sp = (p.pos - grassPrev).len() / dt;
        ms[nm++] = {p.pos, sp, riddenMount() ? 15.f : 11.f, true};
    }
    grassPrev = p.pos;
    for (const auto& a : animals) {
        if (nm >= 40) break;
        if (a.dead || a.gone || a.air > 0.5f || std::fabs(a.pos.x - p.pos.x) > LW || std::fabs(a.pos.y - p.pos.y) > LH) continue;  // only what is on screen
        const float sp = a.vel.len();
        if (sp > 10.f) ms[nm++] = {a.pos, sp, 10.f, false};
    }
    auto byY = [](const Decor& d, float y) { return d.y < y; };
    for (int m = 0; m < nm; m++) {
        const Mover& mv = ms[m];
        if (mv.speed < 10.f) continue;
        const float amp = std::min(1.f, 0.35f + mv.speed / 90.f);
        bool kicked = false;
        for (auto it = std::lower_bound(world.decor.begin(), world.decor.end(), mv.pos.y - 8.f, byY); it != world.decor.end() && it->y <= mv.pos.y + 8.f; ++it) {
            const float dx = it->x - mv.pos.x;
            if (std::fabs(dx) > mv.reach) continue;
            switch (it->kind) {  // plants only: stones, bones and bubbles do not bend
                case D_PEBBLES: case D_SKULL: case D_BUBBLES: case D_DRIFT: case D_LILY: case D_MUSHROOM: case D_BUSH0: case D_BUSH1: continue;
                default: break;
            }
            kicked = reactTo((size_t)(it - world.decor.begin()), dx, amp) || kicked;
        }
        if (kicked && mv.hunter && mv.speed > 70.f && grassPuffCd <= 0.f) {  // a few torn blades fly up when you run through it
            grassPuffCd = 0.16f;
            for (int k = 0; k < 2; k++)
                parts.push_back({p.pos.x + frng.range(-4, 4), p.pos.y, frng.range(0, 3), frng.range(-14, 14), frng.range(-6, 6), frng.range(12, 30), 0.45f, 0.45f, k ? HEX(0x8fd05a) : HEX(0x5aa63f), 1, false});
        }
    }
}

// How far a decor piece leans (degrees) and wobbles because of its last kick.
float Game::grassBend(size_t i) const {
    if (i >= grassT.size()) return 0.f;
    const float age = timeAcc - grassT[i];
    if (age < 0.f || age > GRASS_REST) return 0.f;
    const float f = 1.f - age / GRASS_REST;
    return grassDir[i] * grassAmp[i] * f * 26.f + sinf(age * 22.f) * grassAmp[i] * f * f * 9.f;
}
