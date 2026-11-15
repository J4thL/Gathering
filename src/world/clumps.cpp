// clumps.cpp - the little swaying grass clumps scattered over the ground: wide ones at the foot of the town's buildings, small ones along
// the edges of roads and paving and over the open grass (nearly every grass tile has one). They are drawn in the depth-sorted layer (so a hunter walking through
// is covered up to the shins) and animated with the same 24-frame sway as the grass round the tree roots. Placement is a pure function of
// the tile (no random stream), so the same map always grows the same grass.
#include "core/game.hpp"

void World::genClumps() {
    clumps.clear();
    return;  // the meadow has no grass clumps on top of the ground tile any more (kept as a hook)
    if (isInterior() || mapId == B_HOMESTEAD) return;  // rooms and mines have none; the Homestead is built over by the player
    static std::vector<uint8_t> bld;                   // 1 = a town building's footprint
    bld.assign((size_t)MW * MH, 0);
    if (mapId == B_HOME)
        for (int k = 0; k < 6; k++)
            for (int y = 0; y < BLD_H; y++)
                for (int x = 0; x < BLD_W; x++) {
                    const int tx = COX + BUILDINGS[k].cx + x, ty = COY + BUILDINGS[k].cy + y;
                    if (tx >= 0 && ty >= 0 && tx < MW && ty < MH) bld[(size_t)ty * MW + tx] = 1;
                }
    auto isBld = [&](int tx, int ty) { return tx >= 0 && ty >= 0 && tx < MW && ty < MH && bld[(size_t)ty * MW + tx]; };
    auto road = [&](int tx, int ty) { const uint8_t t = tile[ty][tx]; return t == T_COBBLE || t == T_DIRT; };
    auto add = [&](float x, float y, uint32_t h, int set) { clumps.push_back({x, y, ((h >> 12) & 1023) / 1023.f * 6.2832f, (uint8_t)set}); };
    for (int ty = 1; ty < MH - 1; ty++)
        for (int tx = 1; tx < MW - 1; tx++) {
            const uint8_t t = tile[ty][tx];
            const bool grassy = t == T_GRASS || t == T_DARK;
            if ((!grassy && t != T_COBBLE) || solid[ty][tx] || isBld(tx, ty)) continue;
            if (soilPatchAt(*this, tx, ty)) continue;  // bare soil: nothing grows on it
            const uint32_t h = hash2(tx, ty, 0x6a55u);
            const float roll = (h & 255) / 255.f;
            const float jx = tx * (float)TS + 2.f + (float)((h >> 8) % 12), jy = ty * (float)TS + 6.f + (float)((h >> 20) % 9);
            bool nearBld = false;
            for (int dy = -1; dy <= 1 && !nearBld; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    if (isBld(tx + dx, ty + dy)) { nearBld = true; break; }
            if (nearBld) {  // the foot of a building: grass pushing up along the front, tufts at the sides (a few weeds between the paving stones)
                const bool front = isBld(tx, ty - 1);  // the row just under a building: right up against its base
                if (grassy ? roll < 0.8f : roll < (front ? 0.34f : 0.14f)) {
                    add(jx, front ? ty * (float)TS + 3.f + (float)((h >> 20) % 4) : jy, h, grassy && (h & 0x100000) ? 0 : 1);
                }
            } else if (grassy && (road(tx - 1, ty) || road(tx + 1, ty) || road(tx, ty - 1) || road(tx, ty + 1))) {  // the edge of a road
                if (roll < 0.9f) add(jx, jy, h, (h & 0x100000) ? 0 : 1);
            } else if (grassy) {  // open grass: blades over (nearly) every tile, a second one on many
                if (roll < 0.85f) add(jx, jy, h, 1);
                const uint32_t h2 = hash2(tx, ty, 0x51ed5u);
                if ((h2 & 255) < 90) add(tx * (float)TS + 1.f + (float)((h2 >> 8) % 14), ty * (float)TS + 5.f + (float)((h2 >> 20) % 10), h2, 1);
            }
        }
    std::sort(clumps.begin(), clumps.end(), [](const Clump& a, const Clump& b) { return a.y < b.y; });
}
