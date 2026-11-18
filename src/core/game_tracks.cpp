// game_tracks.cpp - animal footprints and the Tracking skill.
// Every walking animal near the hunter leaves a print every TRACK_STEP pixels. Prints fade; the Tracking skill makes them last longer
// and visible from farther away. Walking up to a fresh print "reads" it once for a little Tracking XP.
#include "core/game.hpp"

static float trackSight(const Game& g) { return TRACK_SIGHT * (1.f + 2.f * g.skillBonus(SK_TRACKING)); }
static float trackLife(const Game& g) { return TRACK_LIFE * (1.f + 2.f * g.skillBonus(SK_TRACKING)); }

void Game::updateTracks(float dt) {
    if (!inWilds()) { tracks.clear(); return; }
    for (size_t i = 0; i < tracks.size();) {
        tracks[i].t += dt;
        if (tracks[i].t >= tracks[i].life) tracks.erase(tracks.begin() + i);
        else i++;
    }
    for (auto& a : animals) {
        if (a.dead || a.gone || a.air > 1.f || (ANIMAL[a.kind].flags & BH_FLIES)) continue;
        if (dist(a.pos, a.printAt) < TRACK_STEP) continue;
        const bool far = dist(a.pos, p.pos) > TRACK_SPAWN_RANGE;
        const bool jump = dist(a.pos, a.printAt) > 60.f;  // first frame / spawn / teleport: no trail across the gap
        a.printAt = a.pos;
        if (far || jump || tracks.size() >= TRACK_MAX) continue;
        tracks.push_back({a.pos, a.kind, 0.f, trackLife(*this), false});
    }
    // reading a fresh print teaches a little
    if (trackReadCd > 0) trackReadCd -= dt;
    else
        for (auto& t : tracks)
            if (!t.read && t.t < t.life * 0.6f && dist(t.p, p.pos) < 14.f) {
                t.read = true;
                grantXp(SK_TRACKING, 2);
                trackReadCd = 1.5f;
                break;
            }
}

void Game::drawTracks() {
    const float sight = trackSight(*this);
    for (auto& t : tracks) {
        const float d = dist(t.p, p.pos);
        if (d > sight) continue;
        const float fresh = 1.f - t.t / t.life;                          // 1 new .. 0 gone
        const float edge = std::min(1.f, (sight - d) / 20.f);           // fades out at the edge of sight
        const int al = (int)(std::max(0.f, std::min(1.f, fresh * edge)) * 150.f);
        if (al < 10) continue;
        const int x = (int)t.p.x - camX, y = (int)t.p.y - camY;
        const u32 col = RGBA(40, 28, 20, al);
        if (t.kind == A_RABBIT) { fillRect(x, y, 1, 1, col); fillRect(x + 2, y + 1, 1, 1, col); }
        else if (t.kind == A_WOLF) { fillRect(x, y, 2, 2, col); fillRect(x + 3, y, 1, 1, col); fillRect(x - 1, y - 1, 1, 1, col); fillRect(x + 2, y - 1, 1, 1, col); }
        else { fillRect(x, y, 1, 2, col); fillRect(x + 2, y, 1, 2, col); }  // a cloven hoof
    }
}
