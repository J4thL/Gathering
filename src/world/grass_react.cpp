// grass_react.cpp - the grass answers to whoever walks through it: tufts, flowers, reeds and the like bend away from a moving hunter or
// animal and then rustle back to rest. Only the little record of the last kick is kept (when, how hard, which way); the renderer turns
// it into a lean and a wobble. The grass touches no gameplay state; bushes (bottom of the file) rustle, drop their snow and flush hares, with their own random stream.
#include "core/game.hpp"
#include "core/logic_util.hpp"

void Game::updateGrass(float dt) {
    const size_t n = world.decor.size();
    if (grassMap != world.mapId || grassT.size() != n) {  // another map: start with calm grass
        grassMap = world.mapId;
        grassT.assign(n, -100.f);
        grassAmp.assign(n, 0.f);
        grassDir.assign(n, 0);
        grassPrev = p.pos;
        grassPuffCd = 0.f;
        bushShedT.assign(n, -1.f);
        bushCd.assign(n, 0.f);
    }
    if (wxKind == WX_BLIZZARD && rainK() > 0.3f)  // a blizzard banks the snow up on the bare shrubs four times as fast
        for (size_t i = 0; i < bushShedT.size(); i++)
            if (bushShedT[i] >= 0.f) bushShedT[i] -= dt * 3.f * rainK();
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
                case D_PEBBLES: case D_DROCK0: case D_DROCK1: case D_DROCK2: case D_DROCK3: case D_SKULL: case D_BUBBLES: case D_DRIFT: case D_LILY: case D_MUSHROOM: case D_FROSTCRACK: case D_TRACKS: case D_BURIEDROCK: continue;
                default: break;
            }
            const size_t di = (size_t)(it - world.decor.begin());
            const bool hit = reactTo(di, dx, amp);
            if (hit && mv.speed >= 14.f) bushBrushed(di, mv.pos, mv.speed, mv.hunter);
            kicked = hit || kicked;
        }
        if (kicked && mv.hunter && mv.speed > 70.f && grassPuffCd <= 0.f) {  // a few torn blades fly up when you run through it
            grassPuffCd = 0.16f;
            for (int k = 0; k < 2; k++)
                parts.push_back({p.pos.x + frng.range(-4, 4), p.pos.y, frng.range(0, 3), frng.range(-14, 14), frng.range(-6, 6), frng.range(12, 30), 0.45f, 0.45f, k ? HEX(0x8fd05a) : HEX(0x5aa63f), 1, false});
        }
    }
}

// How far a decor piece leans (degrees) and wobbles because of its last kick. A bush stays bent longer than grass does, so the way you went shows.
float Game::grassBend(size_t i) const {
    if (i >= grassT.size()) return 0.f;
    const float age = timeAcc - grassT[i];
    if (age < 0.f || age > GRASS_REST * 2.f) return 0.f;
    const float rest = GRASS_REST * (i < world.decor.size() ? std::max(1.f, bushDef(world.decor[i]).rest) : 1.f);
    if (age > rest) return 0.f;
    const float f = 1.f - age / rest;
    return grassDir[i] * grassAmp[i] * f * 26.f + sinf(age * 22.f) * grassAmp[i] * f * f * 9.f;
}

// ---- bushes (bush_react.hpp)
BushDef Game::bushDef(const Decor& d) const { return bushDefFor(d.kind, world.mapId == B_HOMESTEAD ? -1 : rockTheme()); }

float Game::bushSnowK(size_t i) const {
    if (i >= bushShedT.size() || bushShedT[i] < 0.f) return 1.f;
    const float age = timeAcc - bushShedT[i];
    return clampf((age - 0.7f * BUSH_REFILL) / (0.3f * BUSH_REFILL), 0.f, 1.f);  // bare for most of the time, then the snow comes back
}

float Game::bushCoverAt(V2 pos) const {
    float best = 0.f;
    auto byY = [](const Decor& d, float y) { return d.y < y; };
    for (auto it = std::lower_bound(world.decor.begin(), world.decor.end(), pos.y - 4.f, byY); it != world.decor.end() && it->y <= pos.y + 9.f; ++it) {
        if (std::fabs(it->x - pos.x) > 9.f) continue;
        const BushDef d = bushDef(*it);
        if (!d.bush) continue;
        float c = d.cover;
        if (d.sheds) c *= 0.5f + 0.5f * bushSnowK((size_t)(it - world.decor.begin()));  // a shrub that has dropped its snow hides less
        best = std::max(best, c);
    }
    return best;
}

// Someone has pushed through bush `i` at `speed`: it rustles and sheds leaves (or snow, or needles, or dust); a snow shrub brushed hard drops its
// snow and stands bare for a while; and a hare or rabbit lying up in it bolts.
void Game::bushBrushed(size_t i, V2 who, float speed, bool hunter) {
    if (i >= world.decor.size() || i >= bushCd.size()) return;
    const Decor& b = world.decor[i];
    const BushDef d = bushDef(b);
    if (!d.bush || timeAcc < bushCd[i]) return;
    const float near = dist(who, p.pos);
    if (near > 220.f) return;
    bushCd[i] = timeAcc + 0.45f;
    audio.play(S_RUSTLE, clampf(0.12f + speed / 420.f, 0.12f, 0.34f) * (hunter ? 1.f : 0.5f) * clampf(1.2f - near / 220.f, 0.2f, 1.f), 0.18f);
    const int n = d.burst + (speed > 70.f ? 2 : 0);
    for (int k = 0; k < n; k++)
        parts.push_back({b.x + brng.range(-4, 4), b.y - 1.f, brng.range(3, 9), brng.range(-26, 26), brng.range(-10, 8), brng.range(14, 40), 0.55f, 0.55f, k & 1 ? d.leafA : d.leafB, 1, true});
    if (d.sheds && speed >= 40.f && bushSnowK(i) >= 1.f) {  // the snow slides off in a heap and a cloud
        bushShedT[i] = timeAcc;
        for (int k = 0; k < 9; k++)
            parts.push_back({b.x + brng.range(-5, 5), b.y - 2.f, brng.range(2, 8), brng.range(-18, 18), brng.range(-6, 6), brng.range(6, 24), 0.8f, 0.8f, k % 3 ? HEX(0xffffff) : HEX(0xdbe7f6), 2, true});
        for (int k = 0; k < 4; k++) parts.push_back({b.x + brng.range(-3, 3), b.y - 4.f, brng.range(0, 3), brng.range(-14, 14), brng.range(-6, 6), brng.range(10, 36), 0.5f, 0.5f, HEX(0xf4f9ff), 1, false});
    }
    if (hunter && speed >= 30.f)
        for (auto& a : animals) {
            if (a.dead || a.gone || a.air > 0.5f || !(a.kind == A_RABBIT || a.kind == A_SNOWHARE)) continue;
            if (a.st != AS_WANDER && a.st != AS_FEED && a.st != AS_REST) continue;
            if (dist(a.pos, V2(b.x, b.y)) > 30.f) continue;
            a.st = AS_FLEE;
            a.fleeAnimal = false;
            a.stT = brng.range(2.f, 3.5f);
            a.threat = who;
            a.alertT = 0.3f;
            for (int k = 0; k < 4; k++) parts.push_back({a.pos.x + brng.range(-3, 3), a.pos.y - 2.f, brng.range(0, 3), brng.range(-14, 14), brng.range(-6, 6), brng.range(10, 36), 0.5f, 0.5f, d.leafB, 1, false});
        }
}
