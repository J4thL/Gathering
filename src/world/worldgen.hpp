// worldgen.hpp - builds one map: the home map's gates and forest, or a complete biome map. Included at the end of world.hpp.
//
// Every map is MW x MH (240x168) tiles, ringed by a 2-tile wall. The home map is the original 88x60 Wildwood (World::generateCore,
// unchanged) in the centre, surrounded by home forest; its four dirt paths run out to gaps in the edge wall (the gaps are portals, see maps.hpp).
// Each biome map is generated whole from its own rules below: ground, a trail from its gate to a hub landmark with spur trails
// to the other landmarks, scattered props and decorations, the edge wall and resource nodes. The biome tables (what grows
// where, which resources) are the ones the original ring-around-home world used; only the layout was fitted to one map.
// Everything is deterministic for a given world seed and map.
#pragma once
#include <functional>
#include <vector>
#include "world/maps.hpp"
#include "art/tree_lime.hpp"
#include "art/tree_pine.hpp"

inline void World::generateMap(int map) {
    if (map == B_HOMESTEAD) {  // the player's sandbox has its own generator (homestead_gen.hpp)
        generateHomestead();
        return;
    }
    Rng r(seed * 104729u + 977u + (uint32_t)map * 7919u);
    const uint32_t S = seed + (uint32_t)map * 31u;  // per-map salt for the noise fields
    const bool home = map == B_HOME;
    const MapDef& MD = MAPS[map];
    auto inCore = [](int x, int y) { return x >= COX && x < COX + CW && y >= COY && y < COY + CH; };
    auto inMap = [](int x, int y) { return x >= 0 && y >= 0 && x < MW && y < MH; };
    auto inRing = [](int x, int y) { return x < 2 || y < 2 || x >= MW - 2 || y >= MH - 2; };  // the map's edge wall
    auto cl = [](float v, float a, float b) { return v < a ? a : (v > b ? b : v); };

    if (!home) {  // no village here: park the village-only things far outside the map so nothing draws or triggers
        const V2 away(-99999.f, -99999.f);
        village = hut = door = keeper = fire = questGiver = away;
    }
    for (int b = 0; b < B_COUNT; b++) hub[b] = V2(-99999.f, -99999.f);
    hub[map] = V2(MD.hubTx * TS + 8.f, MD.hubTy * TS + 8.f);
    if (!home) spawn = V2(MD.entryTx * TS + 8.f, MD.entryTy * TS + 8.f);

    // keep mask: 1 = trail tile, 2 = cleared zone (no scattered props / solid terrain), 3 = landmark feature
    std::vector<uint8_t> keep((size_t)MW * MH, 0);
    auto K = [&](int x, int y) -> uint8_t& { return keep[(size_t)y * MW + x]; };

    auto baseGround = [](int b) -> uint8_t {
        switch (b) {
            case B_DESERT: return T_SAND;
            case B_SWAMP: return T_MUD;
            case B_SNOW: return T_SNOW;
            case B_MOUNTAIN: return T_STONE;
            default: return T_GRASS;
        }
    };

    // ---- the snowfield, v2 (GEN_V2_SNOW, opt-in): the zone fields, the wind-drift field, the rock ridges, the frozen lakes and the tree thickets read
    // FastNoiseLite instead of the value noise. Off, every function below is exactly the old expression (existing seeds keep their snowfield).
    //   zones   : two OpenSimplex2 fields under a domain warp, so forest / outcrop / slope zones meander instead of blobbing
    //   drifts  : the packed-snow field is warped, then stretched along the wind (x), so the streaks bend and taper like real drifts
    //   ridges  : a level set of a warped OpenSimplex2 field, so ridge lines curve; gaps from a second field
    //   lakes   : ellipses in a warped space, so the ice has lobes and inlets
    //   thickets: the tree-density field flows through the same warp as the zones
    const bool sv2 = map == B_SNOW && (genOff & GEN_V2_SNOW) != 0;
    const bool plateaus = sv2 && (genOff & GEN_V2_PLATEAU) != 0;  // raised snow with a rock face along its south edge instead of thin ridges (5c)
    FastNoiseLite sZA(S + 7201), sZB(S + 7202), sZWarp(S + 7203), sDrift(S + 7204), sDWarp(S + 7205), sRidge(S + 7206), sRWarp(S + 7207), sGap(S + 7208),
        sLake(S + 7209), sTree(S + 7210), sLakeJit(S + 7211);
    auto fbm3 = [](FastNoiseLite& n, float freq) { n.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); n.SetFractalType(FastNoiseLite::FractalType_FBm); n.SetFractalOctaves(3); n.SetFrequency(freq); };
    auto warper = [](FastNoiseLite& n, float amp, float freq) { n.SetDomainWarpType(FastNoiseLite::DomainWarpType_OpenSimplex2); n.SetDomainWarpAmp(amp); n.SetFrequency(freq); };
    if (sv2) {
        fbm3(sZA, 0.0364f); fbm3(sZB, 0.0448f); warper(sZWarp, tune.sZoneWarp, 0.02f);
        fbm3(sDrift, 1.f); warper(sDWarp, tune.sDriftWarp, 0.05f);  // sDrift is sampled in coordinates already scaled for the wind-stretch
        fbm3(sRidge, 0.05f); sRidge.SetFractalType(FastNoiseLite::FractalType_None); warper(sRWarp, tune.sRidgeWarp, 0.04f);  // one octave: long smooth ridge lines, no frayed ends
        fbm3(sGap, 0.08f);  // (this line used to sit inside the comment above, so the ridge gaps never got their frequency: they matter only with plateaus off)
        sLake.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); sLake.SetFrequency(0.07f);
        sLakeJit.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); sLakeJit.SetFrequency(0.22f);
        fbm3(sTree, 0.07f);
    }
    auto fn01 = [](FastNoiseLite& n, float x, float y) { return 0.485f + 0.412f * n.GetNoise(x, y); };  // the fBm calibrated to the value noise's 0..1 spread

    // ------------------------------------------------------------ 0. snowfield zoning
    // Two slow noise fields split the snowfield into zones a few dozen tiles across: dense forest, open tundra, drifted slopes and rocky
    // outcrops. Each zone is a 0..1 weight with soft edges, so they blend into one another; tundra is whatever the other three leave.
    // The ground, the trees, the rocks and the ground detail below all read these weights.
    struct SnowZ { float forest, rock, slope, tundra; };
    auto snowZone = [&](int x, int y) {
        auto ss = [](float e0, float e1, float v) { const float t = v < e0 ? 0.f : v > e1 ? 1.f : (v - e0) / (e1 - e0); return t * t * (3.f - 2.f * t); };
        float a, b;
        if (sv2) {
            float wx = (float)x, wy = (float)y;
            sZWarp.DomainWarp(wx, wy);
            a = fn01(sZA, wx, wy);
            b = fn01(sZB, wx, wy);
        } else {
            a = fbm(x * 0.0364f + 5.f, y * 0.0364f + 17.f, S + 331);
            b = fbm(x * 0.0448f + 31.f, y * 0.0448f + 3.f, S + 332);
        }
        SnowZ z;
        z.forest = ss(0.55f, 0.63f, a);
        const float open = 1.f - z.forest;
        z.rock = open * ss(0.52f, 0.60f, b);
        z.slope = open * ss(0.52f, 0.60f, 1.f - b);
        z.tundra = open * (1.f - z.rock) * (1.f - z.slope);
        return z;
    };

    // ------------------------------------------------------------ 1. ground of a biome map
    auto groundFor = [&](int b, int x, int y) -> uint8_t {
        switch (b) {
            case B_DESERT: {
                float d = fbm(x * 0.06f, y * 0.12f, S + 101);
                float rk = fbm(x * 0.08f + 40.f, y * 0.08f + 40.f, S + 103);
                if (rk > 0.68f) return T_STONE;
                if (d > 0.56f) return T_DUNE;
                return T_SAND;
            }
            case B_SWAMP: {
                float bg = fbm(x * 0.055f, y * 0.055f, S + 202);
                float mr = fbm(x * 0.07f, y * 0.07f, S + 201);
                float dk = fbm(x * 0.09f, y * 0.09f, S + 203);
                if (bg > 0.64f) return T_BOG;
                if (mr > 0.53f) return T_MURK;
                if (dk > 0.55f) return T_DARK;
                return T_MUD;
            }
            case B_SNOW: {
                // packed snow lies in long wind-streaked drifts: the coordinates are bent by a slow noise, then stretched along the wind (x), so the
                // patches come out as tapered streaks with ragged ends instead of round blobs
                float p;
                if (sv2) {
                    float qx = (float)x, qy = (float)y;
                    sDWarp.DomainWarp(qx, qy);
                    p = fn01(sDrift, qx * 0.035f, qy * 0.085f);
                } else {
                    const float wx = fbm(x * 0.05f + 3.f, y * 0.05f + 7.f, S + 321) - 0.5f, wy = fbm(x * 0.05f + 11.f, y * 0.05f + 2.f, S + 322) - 0.5f;
                    p = fbm((x + wx * 16.f) * 0.035f, (y + wy * 16.f) * 0.085f, S + 301);
                }
                // thin ridges of snow-capped rock with gaps in them (the same recipe as the mountains' ridge lines); trails and plazas cut through
                float ridge, gap;
                if (sv2) {
                    float qx = (float)x, qy = (float)y;
                    sRWarp.DomainWarp(qx, qy);
                    const float r0 = sRidge.GetNoise(qx, qy);
                    ridge = 0.5f + tune.sRidgeK * r0;
                    gap = fn01(sGap, (float)x, (float)y);
                    // where the field is nearly flat (a saddle, where two lines would cross, or a peak) the band swells into a blob: leave a gap there, so a ridge ends instead
                    const float gx = sRidge.GetNoise(qx + 1.f, qy) - r0, gy = sRidge.GetNoise(qx, qy + 1.f) - r0;
                    if (tune.sRidgeK * sqrtf(gx * gx + gy * gy) < tune.sRidgeMinGrad) gap = 1.f;
                } else {
                    ridge = fbm(x * 0.065f, y * 0.065f, S + 311);
                    gap = fbm(x * 0.08f + 9.f, y * 0.08f + 9.f, S + 312);
                }
                if (!plateaus && fabsf(ridge - 0.5f) < tune.snowRidgeW && gap < tune.snowRidgeGap) return T_CLIFF;  // a band a few tiles wide: the tidy-up in 5b thins it to a line
                const SnowZ z = snowZone(x, y);  // drifted slopes pack more, the wind-scoured tundra less
                const float PTH = tune.snowPack;
                return p > PTH - (0.13f * z.slope - 0.05f * z.tundra) ? T_SNOWP : T_SNOW;
            }
            default: {  // mountains: cliff massifs, thin ridge lines with gaps, alpine meadows near the gate (the east side)
                float alt = 1.f - x / (float)(MW - 1);  // 0 next to the gate, 1 at the far west edge
                float m = fbm(x * 0.055f, y * 0.055f, S + 403);
                float ridge = fbm(x * 0.07f, y * 0.07f, S + 402);
                float gap = fbm(x * 0.12f + 9.f, y * 0.12f + 9.f, S + 404);
                float meadow = fbm(x * 0.07f + 50.f, y * 0.07f + 50.f, S + 401);
                if (m > 0.66f - 0.05f * alt) return T_CLIFF;
                if (fabsf(ridge - 0.5f) < 0.03f && gap < 0.56f) return T_CLIFF;
                if (alt < 0.5f && meadow > 0.52f - alt * 0.25f) return meadow > 0.64f ? T_DARK : T_GRASS;
                return T_STONE;
            }
        }
    };
    // ---- the home forest, v2 (GEN_V2, opt-in): the same layers (density, moisture, glades, flower fields, lakes, brooks) read FastNoiseLite instead of
    // the value noise. Off, every function below is exactly the old expression, so existing seeds keep their forest tile for tile.
    //   density  : domain-warped OpenSimplex2 fBm (gain calibrated to the old field's spread), so thickets and clearings flow instead of blobbing;
    //              blended back into the old field over 22 tiles next to the village, so the ground meets the original area without a seam
    //   moisture : Voronoi "stands" (cell value, warped) plus the old field's gentle slope: moss, needle and leaf-litter floors come in distinct stands
    //   edges    : glades, flower fields and lakes are ellipses in a warped space, so their outlines lobe and wander instead of being rounded blobs
    //   brooks   : the meander follows a smooth noise at the brook's position instead of a sine of the step count
    const bool v2 = (genOff & GEN_V2) != 0;
    FastNoiseLite fnDens(S + 7101), fnDensWarp(S + 7102), fnStand(S + 7103), fnStandWarp(S + 7105), fnEdge(S + 7104), fnBrook(S + 7106);
    fnDens.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); fnDens.SetFractalType(FastNoiseLite::FractalType_FBm);
    fnDens.SetFractalOctaves(3); fnDens.SetFrequency(tune.fDensFreq);
    fnDensWarp.SetDomainWarpType(FastNoiseLite::DomainWarpType_OpenSimplex2); fnDensWarp.SetDomainWarpAmp(tune.fDensWarp); fnDensWarp.SetFrequency(0.025f);
    fnDensWarp.SetFractalType(FastNoiseLite::FractalType_DomainWarpProgressive); fnDensWarp.SetFractalOctaves(2);
    fnStand.SetNoiseType(FastNoiseLite::NoiseType_Cellular); fnStand.SetCellularReturnType(FastNoiseLite::CellularReturnType_CellValue);
    fnStand.SetFrequency(tune.fStandFreq); fnStand.SetCellularJitter(1.f);
    fnStandWarp.SetDomainWarpType(FastNoiseLite::DomainWarpType_OpenSimplex2); fnStandWarp.SetDomainWarpAmp(tune.fStandWarp); fnStandWarp.SetFrequency(0.045f);
    fnEdge.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); fnEdge.SetFrequency(tune.fEdgeFreq);
    fnBrook.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); fnBrook.SetFrequency(0.12f);
    FastNoiseLite fnJit(S + 7111), fnReach(S + 7112), fnBank(S + 7113);  // the fine edge work: floor jitter, how wide the damp shore is, the brook's frayed bank
    fnJit.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); fnJit.SetFrequency(0.3f);
    fnReach.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); fnReach.SetFrequency(0.35f);
    fnBank.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); fnBank.SetFrequency(0.4f);
    // the ground detail (v2): flower drifts in glades, one colour per stand in flower fields, grove clumping, pine / oak stands
    FastNoiseLite fnPatch(S + 7107), fnFlowerCol(S + 7108), fnGrove(S + 7109), fnStandPine(S + 7110);
    fnPatch.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); fnPatch.SetFractalType(FastNoiseLite::FractalType_FBm); fnPatch.SetFractalOctaves(3); fnPatch.SetFrequency(0.22f);
    fnFlowerCol.SetNoiseType(FastNoiseLite::NoiseType_Cellular); fnFlowerCol.SetCellularReturnType(FastNoiseLite::CellularReturnType_CellValue); fnFlowerCol.SetFrequency(0.14f);
    fnGrove.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); fnGrove.SetFractalType(FastNoiseLite::FractalType_FBm); fnGrove.SetFractalOctaves(3); fnGrove.SetFrequency(0.13f);
    fnStandPine.SetNoiseType(FastNoiseLite::NoiseType_Cellular); fnStandPine.SetCellularReturnType(FastNoiseLite::CellularReturnType_CellValue); fnStandPine.SetFrequency(0.09f);
    auto coreGap = [&](float x, float y) {  // tiles from the original area's rectangle (0 inside it)
        const float dx = std::max(std::max(COX - x, x - (COX + CW - 1)), 0.f), dy = std::max(std::max(COY - y, y - (COY + CH - 1)), 0.f);
        return hypotf(dx, dy);
    };
    auto farFromCore = [&](float x, float y) { const float t = std::min(1.f, coreGap(x, y) / 22.f); return t * t * (3.f - 2.f * t); };
    auto dens = [&](float x, float y) {  // the forest's density: the old field, or its FastNoiseLite replacement
        const float old = fbm((x - COX) * 0.08f, (y - COY) * 0.08f, S + 1);
        if (!v2) return old;
        float wx = x, wy = y;
        fnDensWarp.DomainWarp(wx, wy);
        const float nv = 0.485f + 0.412f * fnDens.GetNoise(wx, wy);
        return old + (nv - old) * farFromCore(x, y);
    };
    auto moist = [&](float x, float y) {  // the wet / dry field: the old one, or Voronoi stands over its slope
        const float old = fbm((x - COX) * 0.045f, (y - COY) * 0.045f, S + 31);
        if (!v2) return old;
        float wx = x, wy = y;
        fnStandWarp.DomainWarp(wx, wy);
        const float nv = 0.5f + 0.26f * fnStand.GetNoise(wx, wy) + (old - 0.5f) * 0.5f;
        return old + (nv - old) * farFromCore(x, y);
    };
    // an ellipse test in a warped space: < 1 is inside. amp is how far (in tiles) the outline may wander.
    auto warpedQ = [&](float x, float y, float cx, float cy, float rx, float ry, float amp) {
        const float ox = x + fnEdge.GetNoise(x, y) * amp, oy = y + fnEdge.GetNoise(x + 211.f, y - 97.f) * amp;
        const float dx = (ox - cx) / rx, dy = (oy - cy) / ry;
        return dx * dx + dy * dy;
    };
    if (home) {  // (kept for the tests, which classify open country and forest by the same field the trees were placed with)
        homeDens.assign((size_t)MW * MH, 0.f);
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) homeDens[(size_t)y * MW + x] = dens((float)x, (float)y);
    } else homeDens.clear();
    // The home forest's floor from two fields. DENSITY is the field that has always set the trees' density (S + 1; its high end is the dark
    // thickets); MOISTURE (S + 31) is new. A fine jitter frays the borders so patches do not come out as smooth blobs.
    //   dense + wet -> moss            dense + dry -> pine needles          dense, mid -> dark grass
    //   woodland edge + dry -> leaf litter (oak woods)    woodland edge + wet -> moss      the rest -> meadow grass
    // how deep inside a glade a tile is: < 1 is the open clearing, up to ~1.5 its thinned rim (the fine noise frays the edge). Returns the
    // lowest value over all glades; `which` is that glade (or -1). The glades themselves are chosen in the home block below.
    auto gladeQ = [&](int x, int y, int* which = nullptr) {
        float best = 99.f;
        int bi = -1;
        for (size_t i = 0; i < glades.size(); i++) {
            const Glade& g = glades[i];
            const float dx = (x - g.cx) / g.rx, dy = (y - g.cy) / g.ry;
            const float q0 = dx * dx + dy * dy;
            if (q0 > 3.2f) continue;
            const float q = v2 ? warpedQ((float)x, (float)y, g.cx, g.cy, g.rx, g.ry, tune.fGladeAmp * std::min(g.rx, g.ry)) + (fbm(x * 0.28f, y * 0.28f, S + 57) - 0.5f) * 0.25f
                               : q0 + (fbm(x * 0.28f, y * 0.28f, S + 57) - 0.5f) * 0.55f;
            if (q < best) { best = q; bi = (int)i; }
        }
        if (which) *which = bi;
        return best;
    };
    // the same idea for the flower fields: < 1 is the field itself (its edge is ragged, on a bigger scale than a glade's)
    auto fieldQ = [&](int x, int y) {
        float best = 99.f;
        for (const FlowerField& f : fields) {
            const float dx = (x - f.cx) / f.rx, dy = (y - f.cy) / f.ry;
            const float q0 = dx * dx + dy * dy;
            if (q0 > 2.6f) continue;
            best = std::min(best, v2 ? warpedQ((float)x, (float)y, f.cx, f.cy, f.rx, f.ry, tune.fFieldAmp * std::min(f.rx, f.ry)) + (fbm(x * 0.18f, y * 0.18f, S + 91) - 0.5f) * 0.25f
                                     : q0 + (fbm(x * 0.18f, y * 0.18f, S + 91) - 0.5f) * 0.5f);
        }
        return best;
    };
    auto homeFloor = [&](int x, int y) -> uint8_t {
        if (gladeQ(x, y) < 1.f || fieldQ(x, y) < 1.f) return T_GRASS;  // a clearing or a flower field is meadow
        const float jit = v2 ? fnJit.GetNoise((float)x, (float)y) * 0.035f : (fbm(x * 0.3f, y * 0.3f, S + 37) - 0.5f) * 0.07f;
        const float d = dens((float)x, (float)y) + jit;
        const float w = moist((float)x, (float)y) + jit;
        if (d > 0.60f) {
            if (w > 0.60f) return T_MOSS;
            if (w < 0.42f) return T_NEEDLES;
            return T_DARK;
        }
        if (d > 0.50f) {
            if (w < 0.44f) return T_LEAF;
            if (w > 0.64f) return T_MOSS;
        }
        return T_GRASS;
    };
    if (home) {  // the home forest around the original area: the same grass / dark-grass noise (continued from the area's own
                 // coordinates, so the meadows and thickets flow across its old edge without a seam), a few lakes
        struct Lake {
            float cx, cy, rx, ry;
        } lakes[] = {{30, 30, 12, 6}, {205, 28, 14, 7}, {30, 140, 12, 6}, {205, 142, 13, 6.5f}, {62, 84, 6, 9}, {180, 84, 6, 9}, {120, 20, 9, 4.5f}, {120, 150, 9, 4.5f}};
        if (!(genOff & GEN_NO_GLADES)) {   // ---- glades: ~14 clearings in the dense forest, away from the village, the lakes, the map edge and each other
            Rng gr(seed * 69061u + 555u);
            for (int att = 0; att < 4000 && glades.size() < 14; att++) {
                const float cx = gr.range(20.f, MW - 20.f), cy = gr.range(16.f, MH - 16.f);
                const float rx = gr.range(4.5f, 7.5f), ry = rx * gr.range(0.7f, 1.0f);
                const float wk = gr.f();  // (drawn every attempt, so the kinds do not depend on how many attempts failed)
                if (cx > COX - 8 && cx < COX + CW + 8 && cy > COY - 8 && cy < COY + CH + 8) continue;  // not by the village
                if (dens(cx, cy) < 0.5f) continue;                                                         // forest clearings only: dense forest around
                bool ok = true;
                for (auto& l : lakes)
                    if (hypotf((cx - l.cx) / (l.rx + rx + 3.f), (cy - l.cy) / (l.ry + ry + 3.f)) < 1.f) ok = false;
                for (auto& o : glades)
                    if (hypotf(cx - o.cx, cy - o.cy) < 24.f) ok = false;
                if (!ok) continue;
                // flowers 3, old oak 2, rune stone 1.5, mushroom ring 2, herb patch 2, hunter's rest 1.5 (out of 12)
                const int kind = wk < 0.25f ? GL_FLOWERS : wk < 0.417f ? GL_OAK : wk < 0.542f ? GL_STONE : wk < 0.708f ? GL_FUNGI : wk < 0.875f ? GL_HERBS : GL_CAMP;
                glades.push_back({cx, cy, rx, ry, kind});
            }
        }
        if (!(genOff & GEN_NO_FIELDS)) {   // ---- flower fields: 3 wide meadows in open country, away from the village, lakes, glades, the edge and each other
            Rng fr(seed * 31337u + 4242u);
            for (int att = 0; att < 3000 && fields.size() < 3; att++) {
                const float cx = fr.range(26.f, MW - 26.f), cy = fr.range(22.f, MH - 22.f);
                const float rx = fr.range(9.f, 13.f), ry = rx * fr.range(0.65f, 0.9f);
                if (cx > COX - 12 && cx < COX + CW + 12 && cy > COY - 12 && cy < COY + CH + 12) continue;  // not by the village
                if (dens(cx, cy) > 0.47f) continue;                                                        // open country only (not in the forest)
                bool ok = true;
                for (auto& l : lakes)
                    if (hypotf((cx - l.cx) / (l.rx + rx + 4.f), (cy - l.cy) / (l.ry + ry + 4.f)) < 1.f) ok = false;
                for (auto& g : glades)
                    if (hypotf(cx - g.cx, cy - g.cy) < g.rx + rx + 4.f) ok = false;
                for (auto& o : fields)
                    if (hypotf(cx - o.cx, cy - o.cy) < 40.f) ok = false;
                if (ok) fields.push_back({cx, cy, rx, ry});
            }
        }
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) {
                if (inCore(x, y)) continue;
                uint8_t t = homeFloor(x, y);
                for (auto& l : lakes) {
                    float dx = (x - l.cx) / l.rx, dy = (y - l.cy) / l.ry;
                    if (v2 ? warpedQ((float)x, (float)y, l.cx, l.cy, l.rx, l.ry, tune.fLakeAmp) + (fbm(x * 0.22f, y * 0.22f, S + 7) - 0.5f) * 0.2f < 1.f
                           : dx * dx + dy * dy + (fbm(x * 0.22f, y * 0.22f, S + 7) - 0.5f) * 0.8f < 1.f) t = T_WATER;
                }
                tile[y][x] = t;
                biome[y][x] = B_HOME;
            }
        if (!(genOff & GEN_NO_STREAMS)) {   // ---- streams: ~9 brooks, each wandering from a spring in the wet forest to the nearest lake (they are shallow and walkable: a
            // path that meets one simply fords it). A path is only kept if it reaches a lake, so no brook ends in the middle of nowhere.
            Rng sr(seed * 40503u + 909u);
            auto nearVillage = [&](float x, float y, float m) { return x > COX - m && x < COX + CW + m && y > COY - m && y < COY + CH + m; };
            int made = 0;
            for (int att = 0; att < 900 && made < 9; att++) {
                const float sx = sr.range(14.f, MW - 14.f), sy = sr.range(12.f, MH - 12.f);
                const float amp = sr.range(0.8f, 1.5f), phase = sr.f() * 6.28f, freq = sr.range(0.16f, 0.30f);
                if (nearVillage(sx, sy, 6.f) || moist(sx, sy) < 0.5f) continue;  // springs are in the wet forest
                const Lake* tgt = nullptr;
                float bestD = 1e9f;
                for (auto& l : lakes) {
                    const float d = hypotf(l.cx - sx, l.cy - sy);
                    if (d >= 22.f && d < bestD) { bestD = d; tgt = &l; }
                }
                if (!tgt || bestD > 95.f) continue;
                bool crosses = false;  // the straight line to the lake must keep clear of the village
                for (int i = 0; i <= 40; i++)
                    if (nearVillage(sx + (tgt->cx - sx) * i / 40.f, sy + (tgt->cy - sy) * i / 40.f, 6.f)) crosses = true;
                if (crosses) continue;
                std::vector<std::pair<int, int>> path;
                float px = sx, py = sy, dirx = tgt->cx - sx, diry = tgt->cy - sy;
                { const float l0 = hypotf(dirx, diry); dirx /= l0; diry /= l0; }
                bool reached = false;
                for (int step = 0; step < 520 && !reached; step++) {
                    float tx = tgt->cx - px, ty = tgt->cy - py;
                    const float dist = hypotf(tx, ty);
                    tx /= dist; ty /= dist;
                    const float m = (v2 ? fnBrook.GetNoise(px, py) * tune.fBrookAmp * amp : sinf(step * freq + phase) * amp) * cl(dist / 16.f, 0.f, 1.f);  // the wander dies away near the lake
                    float vx = tx - ty * m * 0.8f, vy = ty + tx * m * 0.8f;
                    dirx = dirx * 0.65f + vx * 0.35f;
                    diry = diry * 0.65f + vy * 0.35f;
                    const float dl = hypotf(dirx, diry);
                    dirx /= dl; diry /= dl;
                    px += dirx * 0.8f;
                    py += diry * 0.8f;
                    const int ix = (int)px, iy = (int)py;
                    if (ix < 4 || iy < 4 || ix >= MW - 4 || iy >= MH - 4 || nearVillage(px, py, 3.f)) break;
                    if (tile[iy][ix] == T_WATER) { reached = true; break; }
                    if (!path.empty() && path.back().first != ix && path.back().second != iy) path.push_back({ix, path.back().second});  // keep the run 4-connected: no diagonal gaps
                    path.push_back({ix, iy});
                    const bool wide = (v2 ? 0.5f + 0.5f * fnBrook.GetNoise(step * 0.92f, (float)made * 59.f) : fbm(step * 0.11f, (float)made * 7.f, S + 73)) > 0.44f;  // about half the way the brook is two tiles wide
                    const bool horiz = fabsf(dirx) > fabsf(diry);
                    if (wide) path.push_back({horiz ? ix : ix + 1, horiz ? iy + 1 : iy});
                }
                if (!reached || path.size() < 24) continue;
                for (auto& pt : path)
                    if (tile[pt.second][pt.first] != T_WATER && !inCore(pt.first, pt.second)) tile[pt.second][pt.first] = T_SHALLOW;
                made++;
            }
        }
        // lake shores: the ground a tile or two from the water is mossy and damp (the shore is ragged: its width follows a fine noise)
        for (int y = 2; y < MH - 2; y++)
            for (int x = 2; x < MW - 2; x++) {
                if (inCore(x, y) || (tile[y][x] != T_GRASS && tile[y][x] != T_DARK)) continue;
                const int reach = (v2 ? fnReach.GetNoise((float)x, (float)y) > 0.f : fbm(x * 0.35f, y * 0.35f, S + 41) > 0.5f) ? 2 : 1;
                bool near = false;
                for (int dy = -reach; dy <= reach && !near; dy++)
                    for (int dx = -reach; dx <= reach; dx++)
                        if (tile[y + dy][x + dx] == T_WATER) { near = true; break; }
                if (!near && (v2 ? fnBank.GetNoise((float)x, (float)y) > -0.3f : fbm(x * 0.4f, y * 0.4f, S + 43) > 0.38f))  // a brook has a narrower, frayed damp bank
                    for (int dy = -1; dy <= 1 && !near; dy++)
                        for (int dx = -1; dx <= 1; dx++)
                            if (tile[y + dy][x + dx] == T_SHALLOW) { near = true; break; }
                if (near) tile[y][x] = T_MOSS;
            }
        // smoothing: noise leaves single-tile specks and one-tile spurs of a floor in the middle of another. A tile that is mostly surrounded
        // by one other floor (4 or more of its 8 neighbours, and at most 2 of its own kind) takes that floor. Three passes. Water banks, glades,
        // the village and everything that is not a floor are left alone.
        if (!(genOff & GEN_NO_SMOOTH)) {
            auto isFloor = [](uint8_t t) { return t == T_GRASS || t == T_DARK || t == T_LEAF || t == T_MOSS || t == T_NEEDLES || t == T_ROOTS; };
            for (int pass = 0; pass < 3; pass++) {
                std::vector<std::pair<int, uint8_t>> change;
                for (int y = 3; y < MH - 3; y++)
                    for (int x = 3; x < MW - 3; x++) {
                        const uint8_t t = tile[y][x];
                        if (!isFloor(t) || inCore(x, y) || gladeQ(x, y) < 1.5f) continue;
                        int n[T_SHALLOW + 1] = {};
                        bool wet = false;
                        for (int dy = -2; dy <= 2; dy++)
                            for (int dx = -2; dx <= 2; dx++) {
                                const uint8_t o = tile[y + dy][x + dx];
                                if (o == T_WATER || o == T_SHALLOW) wet = true;
                                if (abs(dx) <= 1 && abs(dy) <= 1 && (dx || dy) && isFloor(o)) n[o]++;
                            }
                        if (wet) continue;
                        int best = -1;
                        for (int k = 0; k <= T_SHALLOW; k++)
                            if (k != t && isFloor((uint8_t)k) && n[k] >= 4 && (best < 0 || n[k] > n[best])) best = k;
                        if (best >= 0 && n[t] <= 2) change.push_back({y * MW + x, (uint8_t)best});
                    }
                for (auto& c : change) tile[c.first / MW][c.first % MW] = c.second;
            }
        }
    } else {
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) {
                biome[y][x] = (uint8_t)map;
                tile[y][x] = groundFor(map, x, y);
            }
        if (map == B_SWAMP) {  // bog shores are shallow murk, not a hard edge
            for (int y = 1; y < MH - 1; y++)
                for (int x = 1; x < MW - 1; x++)
                    if (tile[y][x] == T_BOG) {
                        const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                        for (auto& d : d4) {
                            uint8_t& n = tile[y + d[1]][x + d[0]];
                            if (n == T_MUD || n == T_DARK) n = T_MURK;
                        }
                    }
        }
        if (map == B_SNOW) {  // frozen lakes
            struct Lk {
                float cx, cy, rx, ry;
            } lk[] = {{60, 40, 11, 6}, {190, 50, 9, 5}, {30, 110, 8, 4.5f}, {160, 20, 7, 4}, {200, 140, 10, 5.5f}};
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++)
                    for (auto& l : lk) {
                        float dx = (x - l.cx) / l.rx, dy = (y - l.cy) / l.ry;
                        float d = dx * dx + dy * dy + (fbm(x * 0.22f, y * 0.22f, S + 307) - 0.5f) * 0.7f;
                        if (sv2) {  // an ellipse in a warped space: lobes and inlets
                            const float ox = x + sLake.GetNoise((float)x, (float)y) * tune.sLakeAmp, oy = y + sLake.GetNoise(x + 211.f, y - 97.f) * tune.sLakeAmp;
                            const float wdx = (ox - l.cx) / l.rx, wdy = (oy - l.cy) / l.ry;
                            d = wdx * wdx + wdy * wdy + sLakeJit.GetNoise((float)x, (float)y) * 0.1f;
                        }
                        if (d < 1.f) tile[y][x] = T_ICE;
                    }
        }
    }

    // ------------------------------------------------------------ 2. trails, hub plaza, gates
    // In every map the ring is the map's edge wall; the trails are cut through it by an explicit gap.
    auto trailTile = [&](int x, int y) {
        if (!inMap(x, y)) return;
        if (inRing(x, y)) return;
        tile[y][x] = biome[y][x] == B_SNOW ? T_SNOWP : T_DIRT;
        solid[y][x] = false;
        K(x, y) = 1;
    };
    auto clearZone = [&](int cx, int cy, int rad) {
        for (int dy = -rad; dy <= rad; dy++)
            for (int dx = -rad; dx <= rad; dx++) {
                int x = cx + dx, y = cy + dy;
                if (!inMap(x, y) || inRing(x, y) || K(x, y) == 1) continue;
                if (K(x, y) == 0) K(x, y) = 2;
                if (!(home && inCore(x, y)) && tileSolid(tile[y][x])) tile[y][x] = baseGround(biome[y][x]);
            }
    };
    auto stampBlock = [&](int px, int py) {
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 2; b++) trailTile(px + a, py + b);
        clearZone(px, py, 2);
        clearZone(px + 1, py + 1, 2);
    };
    auto carveLine = [&](float x0, float y0, float x1, float y1, float amp, int sk) {
        float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
        if (len < 1.f) return;
        float nx = -dy / len, ny = dx / len;
        int n = (int)len;
        for (int i = 0; i <= n; i++) {
            float t = i / len, w = sinf(i * 0.33f + sk) * amp * std::min(1.f, std::min((float)i, (float)(n - i)) / 6.f);
            stampBlock((int)(x0 + dx * t + nx * w), (int)(y0 + dy * t + ny * w));
        }
    };

    if (home) {
        // The four original paths, continued to the edge of the map with the same wiggle formula; where they meet the map's edge wall
        // the wall is opened (the gaps are the portals to the biome maps).
        const float vx = COX + 44, vy = COY + 30;
        const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int k = 0; k < 4; k++)
            for (int i = 30; i <= 125; i++) {
                float wig = sinf(i * 0.37f + k * 2.1f) * 1.6f;
                int px = (int)(vx + dirs[k][0] * i + (dirs[k][0] ? 0 : wig));
                int py = (int)(vy + dirs[k][1] * i + (dirs[k][1] ? 0 : wig));
                stampBlock(px, py);
            }
        for (size_t i = 0; i < props.size();) {
            const Prop& p = props[i];
            int tx = (int)(p.x / TS), ty = (int)(p.y / TS);
            bool tree = p.kind == P_OAK0 || p.kind == P_OAK1 || p.kind == P_PINE || p.kind == P_ROCK0 || p.kind == P_ROCK1;
            if (inMap(tx, ty) && K(tx, ty) != 0 && tree) {  // the trail clears the trees and rocks standing on it
                solid[ty][tx] = false;  // (rocks get their wider footprint later, in fitRocks)
                props.erase(props.begin() + i);
            } else
                i++;
        }
        for (size_t i = 0; i < decor.size();) {
            int tx = (int)(decor[i].x / TS), ty = (int)(decor[i].y / TS);
            if (inMap(tx, ty) && K(tx, ty) == 1) decor.erase(decor.begin() + i);
            else i++;
        }
    } else {
        // the gate: the trail starts just inside the edge wall at the gap and runs to the hub
        for (int i = 0; i < MD.nPortals; i++) {
            const TileRect& z = MD.portals[i].zone;
            const int side = MD.portals[i].side;  // the edge the gap is in
            const int gx = (z.x0 + z.x1) / 2, gy = (z.y0 + z.y1) / 2;
            const int ix = side == 0 ? MW - 3 : side == 3 ? 2 : gx, iy = side == 1 ? MH - 3 : side == 2 ? 2 : gy;
            carveLine((float)ix, (float)iy, (float)MD.hubTx, (float)MD.hubTy, 1.2f, 4 + map);
        }
        // hub plaza
        for (int dy = -4; dy <= 4; dy++)
            for (int dx = -4; dx <= 4; dx++)
                if (dx * dx + dy * dy <= 12) trailTile(MD.hubTx + dx, MD.hubTy + dy);
        clearZone(MD.hubTx, MD.hubTy, 6);
    }

    // cut every map's edge wall open at its gates (the gap tiles are the portal zones)
    for (int i = 0; i < MD.nPortals; i++) {
        const TileRect& z = MD.portals[i].zone;
        for (int y = z.y0; y <= z.y1; y++)
            for (int x = z.x0; x <= z.x1; x++) {
                tile[y][x] = biome[y][x] == B_SNOW ? T_SNOWP : T_DIRT;
                solid[y][x] = false;
                K(x, y) = 1;
            }
    }

    // ------------------------------------------------------------ helpers for landmarks
    auto clearDisc = [&](float cx, float cy, float rad, uint8_t kv) {
        for (int y = (int)(cy - rad) - 1; y <= (int)(cy + rad) + 1; y++)
            for (int x = (int)(cx - rad) - 1; x <= (int)(cx + rad) + 1; x++) {
                if (!inMap(x, y) || inRing(x, y) || K(x, y) == 1) continue;
                if (hypotf(x - cx, y - cy) > rad) continue;
                if (K(x, y) < kv) K(x, y) = kv;
                if (tileSolid(tile[y][x])) tile[y][x] = baseGround(biome[y][x]);
                solid[y][x] = false;
            }
    };
    // The tiles a landmark's objects block. Trails are cut from the hub to the landmarks after the objects are placed, and a trail clears every
    // tile it runs over, which used to leave an object with no collision: you could walk through the Ice Fisher's Hut, some oasis palms and
    // standing stones. So every blocking tile is recorded here and blocked again once all the trails are cut (end of the landmark stage).
    std::vector<std::pair<int, int>> lmBlock;
    auto holdSolid = [&](int x, int y) {
        if (!inMap(x, y)) return;
        solid[y][x] = true;
        if (K(x, y) < 3) K(x, y) = 3;
        lmBlock.push_back({x, y});
    };
    // solid prop on a tile (w tiles wide, centred on tx)
    auto addP = [&](int kind, int tx, int ty, int w = 1, float ox = 8.f, float oy = 15.f) {
        props.push_back({kind, tx * TS + ox, ty * TS + oy, r.f() * 6.28f});
        for (int i = -(w / 2); i <= w / 2; i++) holdSolid(tx + i, ty);
    };
    auto addDecor = [&](int kind, float x, float y) { decor.push_back({kind, x, y, r.f() * 6.28f}); };
    auto addLandmark = [&](const char* name, int b, float tx, float ty) { landmarks.push_back({name, b, tx * TS + 8.f, ty * TS + 8.f, false}); };
    // trail from the hub towards a landmark, stopping `reach` tiles short of its centre
    auto spur = [&](float tx, float ty, float reach, int sk) {
        float hx = (float)MD.hubTx, hy = (float)MD.hubTy, dx = tx - hx, dy = ty - hy, len = sqrtf(dx * dx + dy * dy);
        if (len <= reach + 1) return;
        carveLine(hx, hy, tx - dx / len * reach, ty - dy / len * reach, 1.4f, sk);
    };

    // ------------------------------------------------------------ 2b. where the snowfield's two big landmarks stand
    // The Watch Cairn and the Frozen Fortress stand in their own zone of the map, at a place that comes from the seed (a separate random stream, so
    // nothing else on the map moves with it): clear of the frozen lakes, far enough from the hub and from each other. When no place passes (it cannot
    // happen on a map this size, but it is cheap to be safe) they stand where they always stood. GEN_FIXED_LANDMARKS keeps the old places for every seed.
    int cairnTx = 190, cairnTy = 60, fortTx = 186, fortTy = 114;
    if (map == B_SNOW && !(genOff & GEN_FIXED_LANDMARKS)) {
        Rng lr(seed * 2246822519u + 0x1A4Du);
        auto iceIn = [&](int x0, int y0, int x1, int y1) {
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++)
                    if (inMap(x, y) && tile[y][x] == T_ICE) return true;
            return false;
        };
        auto apart = [](int ax, int ay, int bx, int by, float d) { return hypotf((float)(ax - bx), (float)(ay - by)) >= d; };
        bool okF = false;
        for (int i = 0; i < 400 && !okF; i++) {  // the fortress: a 17 x 18 tile castle with a cleared yard, gate to the south
            const int x = lr.irange(152, 206), y = lr.irange(98, 124);
            if (iceIn(x - 14, y - 12, x + 14, y + 14) || !apart(x, y, MD.hubTx, MD.hubTy, 42.f)) continue;
            fortTx = x, fortTy = y, okF = true;
        }
        for (int i = 0; i < 400; i++) {  // the cairn: a lookout tower, on the north-east side
            const int x = lr.irange(168, 212), y = lr.irange(36, 80);
            if (iceIn(x - 7, y - 7, x + 7, y + 7) || !apart(x, y, fortTx, fortTy, 34.f) || !apart(x, y, MD.hubTx, MD.hubTy, 42.f)) continue;
            cairnTx = x, cairnTy = y;
            break;
        }
    }

    // ------------------------------------------------------------ 3. landmarks
    if (map == B_DESERT) {
        const int hx = MD.hubTx, hy = MD.hubTy;
        addLandmark("SUNKEN RUINS", B_DESERT, hx, hy);
        clearDisc(hx, hy, 8, 2);
        const int col[8][2] = {{-5, -4}, {-2, -5}, {3, -5}, {6, -3}, {-6, 1}, {6, 3}, {-4, 5}, {4, 6}};
        for (auto& c : col) addP(P_COLUMN, hx + c[0], hy + c[1]);
        addP(P_RUINWALL, hx - 4, hy - 2, 3);
        addP(P_RUINWALL, hx + 2, hy + 4, 3);
        addP(P_RUINWALL, hx - 1, hy - 7, 3);
        addP(P_RUINWALL, hx + 7, hy - 1, 3);
        lanterns.push_back({hx * TS + 8.f, hy * TS - 6.f, 0.7f});  // a brazier in the middle of the plaza
        // the Endless Depths: a worn stair sinking into the plaza floor (delve/endless.hpp)
        props.push_back({P_STAIRS, hx * TS + 8.f, (hy + 2) * TS + 14.f, 1.f});
        gate = V2(hx * TS + 8.f, (hy + 2) * TS + 10.f);
        // oasis
        const float cx = 120, cy = 40, rx = 5.5f, ry = 3.8f;
        addLandmark("OASIS", B_DESERT, cx, cy + 6);
        clearDisc(cx, cy, 11, 2);
        for (int y = (int)(cy - 12); y <= (int)(cy + 12); y++)
            for (int x = (int)(cx - 14); x <= (int)(cx + 14); x++) {
                if (!inMap(x, y) || inRing(x, y) || K(x, y) == 1) continue;
                float dx = (x - cx) / rx, dy = (y - cy) / ry;
                float d = dx * dx + dy * dy + (fbm(x * 0.25f, y * 0.25f, S + 601) - 0.5f) * 0.5f;
                if (d < 1.f) {
                    tile[y][x] = T_WATER;
                    K(x, y) = 3;
                } else if (d < 2.6f) {
                    tile[y][x] = T_GRASS;
                    K(x, y) = 3;
                    if (r.chance(0.30f)) addDecor(r.chance(0.5f) ? D_TUFT : r.irange(0, 2), x * TS + r.range(2, 14), y * TS + r.range(6, 15));
                }
            }
        for (int i = 0; i < 9; i++) {
            float a = i * 0.7f + r.range(-0.2f, 0.2f);
            int px = (int)(cx + cosf(a) * rx * 1.7f), py = (int)(cy + sinf(a) * ry * 1.7f);
            if (inMap(px, py) && tile[py][px] == T_GRASS && K(px, py) != 1 && !solid[py][px]) addP(P_PALM, px, py);
        }
        // bone field
        const float bx = 190, by = 130;
        addLandmark("BONE FIELD", B_DESERT, bx, by);
        clearDisc(bx, by, 6, 2);
        for (int i = 0; i < 16; i++) {
            float a = r.range(0, 6.28f), d = r.range(0.5f, 5.2f);
            addDecor(r.chance(0.55f) ? D_SKULL : D_PEBBLES, (bx + cosf(a) * d) * TS + 8, (by + sinf(a) * d) * TS + 8);
        }
        addP(P_SANDROCK1, (int)bx - 4, (int)by - 2, 1, 8, 14);
        addP(P_SANDROCK0, (int)bx + 4, (int)by + 3, 1, 8, 14);
        // sandstone arch
        const int ax = 150, ay = 28;
        addLandmark("SANDSTONE ARCH", B_DESERT, ax, ay + 2);
        clearDisc(ax, ay, 5, 2);
        props.push_back({P_ARCH, ax * TS + 8.f, ay * TS + 15.f, 0});
        holdSolid(ax - 1, ay);
        holdSolid(ax + 1, ay);
        spur(cx, cy, 9, 1);
        spur(bx, by, 3, 2);
        spur((float)ax, (float)ay, 3, 3);
    } else if (map == B_SWAMP) {
        const int hx = MD.hubTx, hy = MD.hubTy;
        addLandmark("STILT HUT", B_SWAMP, hx, hy + 2);
        clearDisc(hx, hy, 8, 2);
        props.push_back({P_STILTHUT, hx * TS + 8.f, (hy - 2) * TS + 15.f, 0});
        for (int i = -1; i <= 1; i++)
            for (int j = 2; j <= 3; j++) holdSolid(hx + i, hy - j);
        lanterns.push_back({hx * TS + 8.f + 14, (hy - 2) * TS - 18.f, 0.8f});
        addP(P_LOG, hx - 6, hy + 1);
        addP(P_DEADTREE0, hx + 7, hy - 3);
        // witch tree
        const int wx = 50, wy = 110;
        addLandmark("WITCH TREE", B_SWAMP, wx, wy + 3);
        clearDisc(wx, wy, 6, 2);
        props.push_back({P_WITCHTREE, wx * TS + 8.f, wy * TS + 15.f, 0});
        for (int i = -1; i <= 1; i++)
            for (int j = 0; j <= 1; j++) holdSolid(wx + i, wy - j);
        for (int i = 0; i < 4; i++) addDecor(D_SKULL, (wx + r.range(-4, 4)) * TS + 8, (wy + r.range(1, 4)) * TS + 8);
        // sunken graves
        const int gx = 190, gy = 120;
        addLandmark("SUNKEN GRAVES", B_SWAMP, gx, gy);
        clearDisc(gx, gy, 7, 2);
        for (int row = 0; row < 2; row++)
            for (int c = 0; c < 4; c++) addP(P_GRAVE, gx - 4 + c * 2 + (row ? 1 : 0), gy - 1 + row * 3);
        spur((float)wx, (float)(wy + 3), 3, 1);
        spur((float)gx, (float)gy, 4, 2);
    } else if (map == B_SNOW) {
        const int hx = MD.hubTx, hy = MD.hubTy;
        addLandmark("ICE FISHER'S HUT", B_SNOW, hx, hy + 2);
        clearDisc(hx, hy, 8, 2);
        props.push_back({P_ICEHUT, hx * TS + 8.f, (hy - 1) * TS + 15.f, 0});
            lanterns.push_back({hx * TS + 8.f, hy * TS + 3.f, 0.8f});  // the door is lit: a pool of warm light on the snow in front of it
        for (int i = -1; i <= 1; i++)
            for (int j = 1; j <= 2; j++) holdSolid(hx + i, hy - j);
        lanterns.push_back({hx * TS + 8.f - 8, (hy - 1) * TS - 6.f, 0.75f});
        // a small fishing pond next to the hut
        for (int y = hy + 1; y <= hy + 8; y++)
            for (int x = hx - 14; x <= hx - 5; x++) {
                if (!inMap(x, y) || inRing(x, y) || K(x, y) == 1) continue;
                float dx = (x - (hx - 9.5f)) / 4.6f, dy = (y - (hy + 4.5f)) / 3.0f;
                if (dx * dx + dy * dy < 1.f) {
                    tile[y][x] = T_ICE;
                    K(x, y) = 3;
                }
            }
        // frozen lake landmark (the ice itself is generated above); crystals along its shore
        addLandmark("FROZEN LAKE", B_SNOW, 60, 47);
        int placed = 0;
        for (int i = 0; i < 40 && placed < 7; i++) {
            float a = r.range(0, 6.28f);
            int px = (int)(60 + cosf(a) * 12.5f), py = (int)(40 + sinf(a) * 7.2f);
            if (inMap(px, py) && !inRing(px, py) && K(px, py) == 0 && (tile[py][px] == T_SNOW || tile[py][px] == T_SNOWP) && !solid[py][px]) {
                addP(P_CRYSTAL, px, py);
                placed++;
            }
        }
        // watch cairn
        const int cx = cairnTx, cy = cairnTy;
        addLandmark("WATCH CAIRN", B_SNOW, cx, cy + 2);
        clearDisc(cx, cy, 5, 2);
        // the Watch Cairn is a lookout: a stone tower with a beacon on top, the old cairn piled at its foot (reaching it shows the map; see game_logic.cpp)
        props.push_back({P_LOOKOUT, cx * TS + 8.f, (cy + 1) * TS + 12.f, 0.f});
        for (int j = 0; j <= 1; j++) holdSolid(cx, cy + j);
        lanterns.push_back({cx * TS + 8.f, (cy + 1) * TS - 38.f, 1.9f});  // the beacon: seen from a long way off at night
        addP(P_CAIRN, cx + 2, cy + 1);
        addP(P_SNOWROCK, cx - 3, cy + 1, 1, 8, 14);
        addP(P_SNOWROCK, cx + 3, cy - 1, 1, 8, 14);
        spur(60, 47, 4, 1);
        spur((float)cx, (float)cy, 3, 2);
        // the Frozen Dungeon: an ancient fortress of ice, reached by a trail from the hut (delve.hpp)
        const int fx = fortTx, fy = fortTy;
        addLandmark("FROZEN FORTRESS", B_SNOW, fx, fy + 10);
        clearDisc(fx, fy, 14, 2);
        // the fortress sprite is 256 x 288 (16 x 18 tiles): this grid is its footprint (17 columns, j = -8..8 around fx; 18 rows from fy - 9).
        // '#' is solid castle and mound; the three tiles down the middle of the lower rows are the stairs up to the door at (fx, fy + 3)
        static const char* const FORT[18] = {
            ".................",
            "........#........",
            "......#####......",
            "......#####......",
            "....#########....",
            "...###########...",
            "..#############..",
            "..#############..",
            "..#############..",
            ".###############.",
            ".###############.",
            ".###############.",
            ".######...######.",
            ".######...######.",
            "..#####...#####..",
            "..#####...#####..",
            "....###...###....",
            "......#...#......"};
        for (int k = 0; k < 18; k++)
            for (int j = -8; j <= 8; j++) {
                const int x = fx + j, y = fy - 9 + k;
                if (!inMap(x, y)) continue;
                if (FORT[k][j + 8] == '#') {
                    tile[y][x] = T_SNOWP;
                    solid[y][x] = true;
                }
                K(x, y) = 3;
            }
        props.push_back({P_FROSTGATE, fx * TS + 8.f, (fy + 3) * TS + 0.f, 0});  // sorted at the door, so a player on the stairs is drawn in front of the castle
        gate = V2(fx * TS + 8.f, (fy + 3) * TS + 4.f);
        lanterns.push_back({fx * TS + 8.f - 64, (fy + 8) * TS + 4.f, 0.7f});
        lanterns.push_back({fx * TS + 8.f + 64, (fy + 8) * TS + 4.f, 0.7f});
        spur((float)fx, (float)(fy + 10), 2, 3);
    } else if (map == B_MOUNTAIN) {
        const int hx = MD.hubTx, hy = MD.hubTy;
        addLandmark("MOUNTAIN PASS", B_MOUNTAIN, hx, hy);
        clearDisc(hx, hy, 9, 2);
        const int st[6][2] = {{-6, -3}, {-6, 3}, {6, -3}, {6, 3}, {0, -6}, {0, 6}};
        for (auto& s : st) addP(P_STANDING, hx + s[0], hy + s[1]);
        // caves: a cliff outcrop with a mouth in its south face and a trail to the foot of it
        auto makeCave = [&](int tx, int ty, const char* name, int sk, bool mine = false) {
            int y1 = ty;
            clearDisc(tx, ty + 1, 8, 2);
            for (int y = ty - 3; y <= y1; y++)
                for (int x = tx - 4; x <= tx + 4; x++) {
                    if (!inMap(x, y) || inRing(x, y) || K(x, y) == 1) continue;
                    if (y == ty - 3 && (x == tx - 4 || x == tx + 4)) continue;  // rounded corners
                    tile[y][x] = T_CLIFF;
                    solid[y][x] = true;
                    K(x, y) = 3;
                }
            props.push_back({mine ? P_MINEGATE : P_CAVE, tx * TS + 8.f, (y1 + 1) * TS + 0.f, 0});
            if (mine) gate = V2(tx * TS + 8.f, (y1 + 2) * TS + 6.f);  // E here goes down into the Abandoned Mine (delve.hpp)
            addLandmark(name, B_MOUNTAIN, tx, y1 + 2);
            // trail: hub -> a waypoint south of the outcrop -> the cave mouth
            carveLine((float)hx, (float)hy, (float)tx, (float)(y1 + 5), 1.4f, sk);
            carveLine((float)tx, (float)(y1 + 5), (float)tx, (float)(y1 + 1), 0.f, sk);
        };
        makeCave(130, 40, "ECHO CAVE", 1);
        makeCave(130, 128, "ABANDONED MINE", 2, true);
        // summit cairn
        const int sx = 30, sy = 84;
        addLandmark("SUMMIT CAIRN", B_MOUNTAIN, sx, sy + 2);
        clearDisc(sx, sy, 5, 2);
        addP(P_CAIRN, sx, sy);
        addP(P_BOULDER, sx + 3, sy + 1, 1, 8, 14);
        addP(P_BOULDER, sx - 3, sy - 1, 1, 8, 14);
        spur((float)sx, (float)sy, 3, 3);
    }

    for (const auto& b : lmBlock) solid[b.second][b.first] = true;  // the trails cut above may have cleared a landmark object: block it again

    // ------------------------------------------------------------ 4. gate signs (home): one beside each gap, inside the wall
    if (home) {
        struct G {
            int kind, tx, ty;
        } gates[4] = {{P_BSIGN0, COX + CW + 2, COY + 26}, {P_BSIGN1, COX + 48, COY + CH + 2}, {P_BSIGN2, COX + 48, COY - 2}, {P_BSIGN3, COX - 3, COY + 26}};
        for (auto& g : gates) {
            clearDisc(g.tx, g.ty, 1.5f, 3);
            addP(g.kind, g.tx, g.ty);
        }
    }

    if (map == B_SNOW && sv2)  // the zone weights, for the renderer (snowForest / snowSlope in world.hpp)
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) {
                const SnowZ z = snowZone(x, y);
                snowForest[y][x] = (uint8_t)(z.forest * 255.f + 0.5f);
                snowSlope[y][x] = (uint8_t)(z.slope * 255.f + 0.5f);
            }
    // ------------------------------------------------------------ 5c. snow plateaus (GEN_V2_PLATEAU)
    // Raised ground in the snowfield: a noise-shaped mesa, flat on top, with a tall rock face (two tiles of T_CLIFF) along its south edge and only a thin
    // lip on the other three sides. The way up is a ramp: a gap two tiles wide in the face. Collision stays on tiles, except the lip: the plateau's own edge
    // tiles carry a strip (edge, see World::blocked) so nothing walks off the side, and the ramp is the one place without one.
    //   shape   : warped OpenSimplex2 fBm, stretched along x, cut at a level; smoothed, small gaps closed, kept clear of trails, landmarks, props, ice
    //   face    : below every bottom edge; a column whose two face tiles are not free is cut off the plateau, until nothing is left to cut
    //   ramps   : one to three per plateau, on a straight stretch of face with free ground below; a plateau no ramp reaches is levelled
    if (plateaus) {
        const int N = MW * MH;
        auto id = [&](int x, int y) { return y * MW + x; };
        std::vector<char> P(N, 0), bad(N, 0), tmp(N, 0), pt(N, 0);
        auto Pg = [&](int x, int y) -> bool { return inMap(x, y) && P[id(x, y)]; };
        for (const Prop& p : props) {
            const int tx = (int)(p.x / TS), ty = (int)((p.y - 1.f) / TS);
            if (inMap(tx, ty)) pt[id(tx, ty)] = 1;
        }
        FastNoiseLite pN(S + 7231), pW(S + 7232);
        pN.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); pN.SetFractalType(FastNoiseLite::FractalType_FBm); pN.SetFractalOctaves(2); pN.SetFrequency(1.f);
        warper(pW, tune.sPlatWarp, 0.03f);
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) {
                float wx = (float)x, wy = (float)y;
                pW.DomainWarp(wx, wy);
                // the cut follows the zones: rocky outcrops lower it (the shapes swell and merge into massifs), forest and open tundra raise it (few, small mesas,
                // wide open ground between the ranges)
                const SnowZ pz = snowZone(x, y);
                const float cut = tune.sPlatCut + tune.sPlatZone * (0.40f - pz.rock + 0.30f * pz.forest + 0.10f * pz.tundra);
                // and so does their scale: in the outcrops the field is sampled at about half the frequency (one big massif where the open ground has several small mesas)
                const float f1 = pN.GetNoise(wx * tune.sPlatFreq * 0.85f, wy * tune.sPlatFreq * 2.2f);
                const float f2 = pN.GetNoise(wx * tune.sPlatFreq * 0.85f * tune.sPlatBig + 91.f, wy * tune.sPlatFreq * 2.2f * tune.sPlatBig + 37.f);
                P[id(x, y)] = 0.5f + 0.5f * (f1 + (f2 - f1) * pz.rock) > cut;
                const uint8_t t = tile[y][x];
                if ((t != T_SNOW && t != T_SNOWP) || K(x, y) != 0 || solid[y][x] || x < 5 || y < 5 || x >= MW - 5 || y >= MH - 6) bad[id(x, y)] = 1;
            }
        auto smooth = [&]() {
            tmp = P;
            for (int y = 1; y < MH - 1; y++)
                for (int x = 1; x < MW - 1; x++) {
                    int c = 0;
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++) c += P[id(x + dx, y + dy)];
                    tmp[id(x, y)] = c >= 5;
                }
            P = tmp;
        };
        smooth();
        smooth();
        for (int pass = 0; pass < 2; pass++) {  // a wide majority along x: a step in an edge is at least four tiles long, so faces run level, not as a staircase
            tmp = P;
            for (int y = 1; y < MH - 1; y++)
                for (int x = 3; x < MW - 3; x++) {
                    int c = 0;
                    for (int dx = -3; dx <= 3; dx++) c += P[id(x + dx, y)];
                    tmp[id(x, y)] = c >= 4;
                }
            P = tmp;
        }
        for (int x = 0; x < MW; x++) {  // a gap of one or two tiles between two plateaus (down a column, along a row) is closed: there is no room for a face or a path in it
            int last = -100;
            for (int y = 0; y < MH; y++)
                if (P[id(x, y)]) {
                    if (y - last - 1 > 0 && y - last - 1 < 3) for (int k = last + 1; k < y; k++) P[id(x, k)] = 1;
                    last = y;
                }
        }
        for (int y = 0; y < MH; y++) {
            int last = -100;
            for (int x = 0; x < MW; x++)
                if (P[id(x, y)]) {
                    if (x - last - 1 > 0 && x - last - 1 < 3) for (int k = last + 1; k < x; k++) P[id(k, y)] = 1;
                    last = x;
                }
        }
        {  // a closing with a 3 x 3 square first: an inside corner where two arms meet is filled in, so two parts never join through a one-tile pinch
            std::vector<char> dil(N, 0);
            for (int y = 1; y < MH - 1; y++)
                for (int x = 1; x < MW - 1; x++)
                    for (int dy = -1; dy <= 1 && !dil[id(x, y)]; dy++)
                        for (int dx = -1; dx <= 1; dx++)
                            if (P[id(x + dx, y + dy)]) { dil[id(x, y)] = 1; break; }
            for (int y = 1; y < MH - 1; y++)
                for (int x = 1; x < MW - 1; x++) {
                    bool all = true;
                    for (int dy = -1; dy <= 1 && all; dy++)
                        for (int dx = -1; dx <= 1; dx++)
                            if (!dil[id(x + dx, y + dy)]) { all = false; break; }
                    if (all) P[id(x, y)] = 1;
                }
        }
        {  // no tongue or spur narrower than four tiles: an opening with a 4 x 4 square (what no such square fits in goes, what one fits in stays whole)
            std::vector<char> keep(N, 0);
            for (int y = 0; y + 3 < MH; y++)
                for (int x = 0; x + 3 < MW; x++) {
                    bool all = true;
                    for (int dy = 0; dy < 4 && all; dy++)
                        for (int dx = 0; dx < 4 && all; dx++) all = P[id(x + dx, y + dy)] != 0;
                    if (all)
                        for (int dy = 0; dy < 4; dy++)
                            for (int dx = 0; dx < 4; dx++) keep[id(x + dx, y + dy)] = 1;
                }
            P = keep;
        }
        {  // nothing on or within two tiles of what is built, laid, frozen or growing there
            std::vector<char> bd(N, 0);
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++)
                    if (bad[id(x, y)])
                        for (int dy = -2; dy <= 2; dy++)
                            for (int dx = -2; dx <= 2; dx++)
                                if (inMap(x + dx, y + dy)) bd[id(x + dx, y + dy)] = 1;
            for (int i = 0; i < N; i++)
                if (bd[i]) P[i] = 0;
        }
        auto components = [&](std::vector<std::vector<int>>& out) {
            out.clear();
            std::vector<char> seenP(N, 0);
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++) {
                    if (!P[id(x, y)] || seenP[id(x, y)]) continue;
                    std::vector<int> cells{id(x, y)};
                    seenP[id(x, y)] = 1;
                    for (size_t h = 0; h < cells.size(); h++) {
                        const int cx = cells[h] % MW, cy = cells[h] / MW;
                        const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                        for (auto& o : d4) {
                            const int nx = cx + o[0], ny = cy + o[1];
                            if (!Pg(nx, ny) || seenP[id(nx, ny)]) continue;
                            seenP[id(nx, ny)] = 1;
                            cells.push_back(id(nx, ny));
                        }
                    }
                    out.push_back(cells);
                }
        };
        // a plateau tile with fewer than two plateau neighbours is a spur; a piece smaller than the minimum, or thinner than 4 x 3 tiles, is not worth a face
        auto tidy = [&]() {
            bool ch = false;
            {  // nothing thinner than two tiles either way: the lip strips (3 px each side) would leave a one-tile lane too narrow for the hunter's 8 px grid
                std::vector<char> keep(N, 0);
                for (int y = 0; y + 1 < MH; y++)
                    for (int x = 0; x + 1 < MW; x++)
                        if (P[id(x, y)] && P[id(x + 1, y)] && P[id(x, y + 1)] && P[id(x + 1, y + 1)]) keep[id(x, y)] = keep[id(x + 1, y)] = keep[id(x, y + 1)] = keep[id(x + 1, y + 1)] = 1;
                for (int i = 0; i < N; i++)
                    if (P[i] && !keep[i]) P[i] = 0, ch = true;
            }
            for (int pass = 0; pass < 3; pass++)
                for (int y = 0; y < MH; y++)
                    for (int x = 0; x < MW; x++)
                        if (P[id(x, y)] && (int)Pg(x + 1, y) + (int)Pg(x - 1, y) + (int)Pg(x, y + 1) + (int)Pg(x, y - 1) < 2) P[id(x, y)] = 0, ch = true;
            std::vector<std::vector<int>> cs;
            components(cs);
            for (auto& cells : cs) {
                int x0 = MW, x1 = 0, y0 = MH, y1 = 0;
                for (int c : cells) { x0 = std::min(x0, c % MW); x1 = std::max(x1, c % MW); y0 = std::min(y0, c / MW); y1 = std::max(y1, c / MW); }
                if ((int)cells.size() < tune.sPlatMin || x1 - x0 < 3 || y1 - y0 < 2) {
                    for (int c : cells) P[c] = 0;
                    ch = true;
                }
            }
            return ch;
        };
        // the two tiles below a bottom edge must be free snow; where they are not, that column of the plateau is cut off
        auto faceCut = [&]() {
            bool ch = false;
            for (int y = 3; y < MH - 3; y++)
                for (int x = 0; x < MW; x++) {
                    if (!P[id(x, y)] || Pg(x, y + 1)) continue;
                    bool ok = true;
                    for (int k = 1; k <= 2 && ok; k++) {
                        const int yy = y + k;
                        ok = inMap(x, yy) && !inRing(x, yy) && !P[id(x, yy)] && (tile[yy][x] == T_SNOW || tile[yy][x] == T_SNOWP) && !solid[yy][x] && K(x, yy) == 0 && !pt[id(x, yy)];
                    }
                    if (ok && inMap(x, y + 3) && P[id(x, y + 3)]) ok = false;  // another plateau does not start right at the foot of a face: a tile of plain ground between them
                    if (!ok) P[id(x, y)] = 0, ch = true;
                }
            return ch;
        };
        auto stabilise = [&]() {
            for (int it = 0; it < 16; it++) {
                bool ch = tidy();
                ch = faceCut() || ch;
                if (!ch) break;
            }
        };
        // a stretch of bottom edge shorter than three tiles would be a one- or two-tile stub of face: cut it off (a few rounds, so a diagonal flank is
        // trimmed back to a vertical side rather than eaten away)
        auto runCut = [&]() {
            bool ch = false;
            for (int y = 0; y < MH - 1; y++)
                for (int x = 0; x < MW;) {
                    if (!(P[id(x, y)] && !Pg(x, y + 1))) { x++; continue; }
                    const int x0 = x;
                    while (x < MW && P[id(x, y)] && !Pg(x, y + 1)) x++;
                    if (x - x0 < 3) {
                        for (int k = x0; k < x; k++) P[id(k, y)] = 0;
                        ch = true;
                    }
                }
            return ch;
        };
        for (int rp = 0; rp < 5; rp++) {
            stabilise();
            if (!runCut()) break;
        }
        stabilise();
        // Laying is done in up to four attempts. After each one the plateaus are walked with the game's own collision test (8 px cells, a 4 px body) from the foot of
        // every ramp; a piece of top that cannot be reached that way (a one-tile pinch between two arms, say, where the lip strips and a rock face leave no room) is
        // taken off the shape and the laying starts again from the snapshot below. The tile-level reach check inside an attempt cannot see pinches like that.
        const std::vector<uint8_t> tileSnap(&tile[0][0], &tile[0][0] + (size_t)MW * MH), keepSnap = keep;
        for (int attempt = 0; attempt < 4; attempt++) {
            if (attempt > 0) {  // back to the ground as it was before the first laying, with the shape as it now stands
                memcpy(&tile[0][0], tileSnap.data(), tileSnap.size());
                keep = keepSnap;
                memset(plat, 0, sizeof(plat));
                memset(edge, 0, sizeof(edge));
                for (int rp = 0; rp < 5; rp++) {
                    stabilise();
                    if (!runCut()) break;
                }
                stabilise();
            }
            // lay it: the plateau's tiles, then the face under every bottom edge
            std::vector<uint8_t> orig(N, 0), openedK(N, 0);
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++)
                    if (P[id(x, y)]) {
                        plat[y][x] = 1;
                        if (K(x, y) == 0) K(x, y) = 2, openedK[id(x, y)] = 1;  // the top of a plateau stays open: no trees or rocks to split it (the hunter walks it end to end)
                    }
            for (int y = 0; y < MH - 3; y++)
                for (int x = 0; x < MW; x++)
                    if (P[id(x, y)] && !Pg(x, y + 1))
                        for (int k = 1; k <= 2; k++) {
                            orig[id(x, y + k)] = tile[y + k][x];
                            tile[y + k][x] = T_CLIFF;
                            K(x, y + k) = 3;
                        }
            auto levelOff = [&](const std::vector<int>& cells) {  // take a plateau away again
                for (int c : cells) {
                    const int x = c % MW, y = c / MW;
                    if (!Pg(x, y + 1) || plat[y + 1][x] == 2)  // the face (or the ramp) below a bottom edge goes back to the snow it was
                        for (int k = 1; k <= 2; k++)
                            if (orig[id(x, y + k)]) {
                                tile[y + k][x] = orig[id(x, y + k)];
                                plat[y + k][x] = 0;
                                K(x, y + k) = 0;
                            }
                }
                for (int c : cells) {
                    P[c] = 0;
                    plat[c / MW][c % MW] = 0;
                    if (openedK[c]) K(c % MW, c / MW) = 0;
                }
            };
            auto walkable = [&](int x, int y) { return inMap(x, y) && !inRing(x, y) && !solid[y][x] && !tileSolid(tile[y][x]); };
            // the ramps
            std::vector<std::vector<int>> cs;
            components(cs);
            for (auto& cells : cs) {
                struct Cand { int x, y; uint32_t h; };
                std::vector<Cand> cand;
                for (int c : cells) {
                    const int x = c % MW, y = c / MW;
                    bool run = true;
                    for (int k = -1; k <= 2; k++) run = run && Pg(x + k, y) && !Pg(x + k, y + 1);  // four columns of straight face: two ramp tiles, a wall on each side
                    if (run && walkable(x, y + 3) && walkable(x + 1, y + 3) && walkable(x, y + 4) && walkable(x + 1, y + 4)) cand.push_back({x, y, hash2(x, y, S + 7240u)});
                }
                std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) { return a.h < b.h; });
                const int want = 1 + (cells.size() > 160) + (cells.size() > 400) + (cells.size() > 800);
                std::vector<Cand> got;
                for (const Cand& c : cand) {
                    bool far = true;
                    for (const Cand& g : got) far = far && abs(g.x - c.x) + abs(g.y - c.y) >= 10;
                    if (!far) continue;
                    got.push_back(c);
                    if ((int)got.size() >= want) break;
                }
                if (got.empty()) { levelOff(cells); continue; }
                for (const Cand& c : got) {
                    for (int k = 1; k <= 2; k++)
                        for (int dx = 0; dx < 2; dx++) {
                            tile[c.y + k][c.x + dx] = T_SNOWP;
                            plat[c.y + k][c.x + dx] = 2;
                            K(c.x + dx, c.y + k) = 2;
                        }
                    for (int k = 3; k <= 5; k++)  // and no trees or rocks on the way down from it
                        for (int dx = -1; dx <= 2; dx++)
                            if (inMap(c.x + dx, c.y + k) && K(c.x + dx, c.y + k) == 0) K(c.x + dx, c.y + k) = 2;
                }
            }
            // every plateau must be reachable from the hub the way the hunter walks (a step between a plateau and plain ground is only possible on a ramp)
            {
                std::vector<char> reach(N, 0);
                std::vector<int> q;
                const int sx = MD.hubTx, sy = MD.hubTy;
                if (walkable(sx, sy)) { q.push_back(id(sx, sy)); reach[id(sx, sy)] = 1; }
                for (size_t h = 0; h < q.size(); h++) {
                    const int cx = q[h] % MW, cy = q[h] / MW;
                    const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (auto& o : d4) {
                        const int nx = cx + o[0], ny = cy + o[1];
                        if (!walkable(nx, ny) || reach[id(nx, ny)]) continue;
                        const int a = plat[cy][cx], b = plat[ny][nx];
                        if ((a == 1) != (b == 1) && a != 2 && b != 2) continue;
                        reach[id(nx, ny)] = 1;
                        q.push_back(id(nx, ny));
                    }
                }
                components(cs);
                for (auto& cells : cs) {
                    bool any = false;
                    for (int c : cells) any = any || reach[c];
                    if (!any) levelOff(cells);
                }
            }
            // the lip strips: every side of a plateau tile that looks onto plain ground (not onto a face, a ramp or more plateau)
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++) {
                    if (plat[y][x] != 1) continue;
                    const int d4[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};  // N E S W = bits 0..3
                    for (int k = 0; k < 4; k++) {
                        const int nx = x + d4[k][0], ny = y + d4[k][1];
                        if (!inMap(nx, ny) || (plat[ny][nx] == 0 && tile[ny][nx] != T_CLIFF)) edge[y][x] |= (uint8_t)(1 << k);
                    }
                }
            {  // can the hunter walk every piece of top? flood from the ramps on 8 px cells (the test the self-tests and --worldcheck use)
                const int CELLW = 8, gw = MW * TS / CELLW, gh = MH * TS / CELLW;
                std::vector<char> sn((size_t)gw * gh, 0);
                std::vector<int> qq;
                auto pas = [&](int cx, int cy) { return cx >= 0 && cy >= 0 && cx < gw && cy < gh && !blocked(cx * CELLW + CELLW / 2.f, cy * CELLW + CELLW / 2.f, 4.f); };
                for (int y = 0; y < MH; y++)
                    for (int x = 0; x < MW; x++)
                        if (plat[y][x] == 2)
                            for (int a2 = 0; a2 < 2; a2++)
                                for (int b2 = 0; b2 < 2; b2++) {
                                    const int cx = x * 2 + a2, cy = y * 2 + b2;
                                    if (pas(cx, cy) && !sn[(size_t)cy * gw + cx]) sn[(size_t)cy * gw + cx] = 1, qq.push_back(cy * gw + cx);
                                }
                for (size_t h = 0; h < qq.size(); h++) {
                    const int cx = qq[h] % gw, cy = qq[h] / gw;
                    const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (auto& o : d4) {
                        const int nx = cx + o[0], ny = cy + o[1];
                        if (pas(nx, ny) && !sn[(size_t)ny * gw + nx]) sn[(size_t)ny * gw + nx] = 1, qq.push_back(ny * gw + nx);
                    }
                }
                bool cut = false;
                for (int y = 0; y < MH; y++)
                    for (int x = 0; x < MW; x++) {
                        if (plat[y][x] != 1) continue;
                        bool open = false, got = false;
                        for (int a2 = 0; a2 < 2; a2++)
                            for (int b2 = 0; b2 < 2; b2++) {
                                const int cx = x * 2 + a2, cy = y * 2 + b2;
                                if (!pas(cx, cy)) continue;
                                open = true;
                                got = got || sn[(size_t)cy * gw + cx];
                            }
                        if (open && !got) P[id(x, y)] = 0, cut = true;
                    }
                if (!cut || attempt == 3) break;
            }
        }
        for (size_t i = 0; i < decor.size();) {  // nothing lies on a face or a ramp
            const int tx = (int)(decor[i].x / TS), ty = (int)(decor[i].y / TS);
            if (inMap(tx, ty) && (tile[ty][tx] == T_CLIFF || plat[ty][tx] == 2)) decor.erase(decor.begin() + i);
            else i++;
        }
    }

    // ------------------------------------------------------------ 5. scatter trees, rocks, plants (biome maps)
    auto tileProp = [&](int kind, int tx, int ty) {
        props.push_back({kind, tx * TS + 8.f + r.range(-2, 2), ty * TS + 15.f, r.f() * 6.28f});
        solid[ty][tx] = true;
    };
    auto rockProp = [&](int kind, int tx, int ty) {
        props.push_back({kind, tx * TS + 8.f, ty * TS + 14.f, 0});
        solid[ty][tx] = true;
    };
    for (int ty = 2; ty < MH - 2; ty++)
            for (int tx = 2; tx < MW - 2; tx++) {
                if (K(tx, ty) != 0 || solid[ty][tx] || (home && inCore(tx, ty))) continue;
                // every home-forest tile draws from a random stream of its own (seeded from the tile's position), so what is placed on a tile
                // never depends on what happened on the tiles before it: adding or removing a layer (a stream, a glade) leaves every tree it
                // does not touch exactly where it was
                if (home) r = Rng(hash2(tx, ty, S + 4001u) * 2654435761u + 977u);
                uint8_t t = tile[ty][tx];
                if (tileSolid(t)) continue;
                const float fx = tx * TS, fy = ty * TS;
                auto dx = [&]() { return fx + r.range(2, 14); };
                auto dy = [&]() { return fy + r.range(6, 15); };
                if (home && t == T_SHALLOW) {  // a brook: lilies and reeds, nothing solid
                    if (r.chance(0.04f)) addDecor(D_LILY, dx(), dy());
                    continue;
                }
                if (home) {  // the home forest outside the original area: same recipe as inside it
                    float ff = dens((float)tx, (float)ty);  // the same field the original area used, so the forest density flows on
                    float pTree = 0.014f + 0.36f * cl((ff - 0.5f) / 0.2f, 0, 1);
                    int gi = -1;
                    const float gq = gladeQ(tx, ty, &gi);
                    if (gq < 1.f) {  // inside a clearing: no trees or rocks, flowers in drifting clusters (the flower glades are carpeted)
                        const bool flowery = glades[gi].kind == GL_FLOWERS;
                        const bool patch = (v2 ? fn01(fnPatch, (float)tx, (float)ty) : fbm(tx * 0.45f, ty * 0.45f, S + 61)) > (flowery ? 0.42f : 0.56f);
                        if (r.chance(patch ? (flowery ? 0.70f : 0.50f) : 0.07f)) addDecor(r.irange(D_FLOWER0, D_FLOWER2), dx(), dy());
                        else if (r.chance(0.09f)) addDecor(D_TUFT, dx(), dy());
                        continue;
                    }
                    const float fq = fieldQ(tx, ty);
                    if (fq < 1.f) {  // inside a flower field: no trees or rocks, flowers thickest in the middle, in drifts of one colour
                        const float depth = 1.f - fq;  // 0 at the edge .. 1 in the middle
                        if (r.chance(std::min(0.95f, 0.35f + 0.9f * depth))) {
                            // patches a few tiles across: several colours in one view (v2: Voronoi stands, one colour each, with ragged borders)
                            const float cn = v2 ? 0.5f + 0.5f * fnFlowerCol.GetNoise(tx + fnEdge.GetNoise(tx + 53.f, (float)ty) * 2.f, ty + fnEdge.GetNoise((float)tx, ty - 71.f) * 2.f)
                                                : fbm(tx * 0.17f, ty * 0.17f, S + 93);
                            const float c1 = v2 ? 0.25f : 0.41f, c2 = v2 ? 0.50f : 0.50f, c3 = v2 ? 0.75f : 0.59f;
                            static const int FK[4] = {D_FLOWER0, D_FLOWER1, D_VIOLET, D_POPPY};  // orange, cream, violet, red
                            int fk = cn < c1 ? D_POPPY : cn < c2 ? D_FLOWER0 : cn < c3 ? D_FLOWER1 : D_VIOLET;
                            if (r.chance(0.12f)) fk = FK[r.irange(0, 3)];  // a little mixing
                            addDecor(fk, dx(), dy());
                            if (depth > 0.25f && r.chance(0.75f)) addDecor(fk, dx(), dy());  // thicker towards the middle: two and three to a tile
                            if (depth > 0.55f && r.chance(0.55f)) addDecor(fk, dx(), dy());
                        } else if (r.chance(0.12f)) addDecor(D_TUFT, dx(), dy());
                        continue;
                    }
                    if (fq < 1.4f) pTree *= 0.3f;  // the rim of a field: only a few trees
                    if (gq < 1.5f) pTree *= 0.35f;  // the rim of a clearing: the forest thins out
                    // groves: a clumping field makes the trees gather in stands with open ground between them (it averages out, so the
                    // forest keeps its overall density); a second field makes a stand mostly pines or mostly oaks
                    const float groveK = cl(((v2 ? fn01(fnGrove, (float)tx, (float)ty) : fbm(tx * 0.13f, ty * 0.13f, S + 81)) - 0.42f) / 0.2f, 0.f, 1.f);
                    pTree = std::min(0.5f, pTree * (0.3f + 1.7f * groveK));
                    const float standPine = v2 ? 0.12f * fnStandPine.GetNoise((float)tx, (float)ty) : (fbm(tx * 0.10f, ty * 0.10f, S + 83) - 0.5f) * 0.6f;  // v2: Voronoi stands
                    // in open country (not thicket) a tree keeps its distance from the next one: lone trees, not a dotted carpet
                    bool crowded = false;
                    if (ff < 0.55f)
                        for (int yy = ty - 2; yy <= ty + 2 && !crowded; yy++)
                            for (int xx = tx - 2; xx <= tx + 2; xx++)
                                if (inMap(xx, yy) && solid[yy][xx]) { crowded = true; break; }
                    if (crowded) pTree = 0.f;
                    const bool thicket = ff >= 0.55f || t == T_ROOTS || t == T_MOSS || t == T_NEEDLES;
                    if (r.chance(pTree)) {  // pines like needles, roots and dark thickets; oaks like leaf litter, moss and meadows
                        const float pPine = cl((t == T_NEEDLES ? 0.88f : t == T_ROOTS ? 0.62f : t == T_DARK ? 0.55f : t == T_MOSS ? 0.30f : t == T_LEAF ? 0.04f : 0.12f) + standPine, 0.02f, 0.97f);
                        const bool oakOnly = t == T_DARK || t == T_NEEDLES || t == T_ROOTS;  // (their oaks are always the dark-leaved look)
                        tileProp(r.chance(pPine) ? P_PINE : (oakOnly || r.chance(0.5f) ? P_OAK0 : P_OAK1), tx, ty);
                    }
                    else if (r.chance(t == T_ROOTS ? 0.030f : t == T_MOSS ? 0.008f : thicket ? 0.004f : 0.0012f)) tileProp(r.chance(0.5f) ? P_DEADTREE0 : P_DEADTREE1, tx, ty);  // dead trees stand in the gloomy ground
                    else if (thicket && r.chance(0.0035f)) {  // a fallen log
                        props.push_back({P_LOG, tx * TS + 8.f, ty * TS + 14.f, 0});
                        solid[ty][tx] = true;
                    }
                    else if (r.chance(thicket ? 0.0075f : 0.0020f)) addDecor(D_STUMP, tx * TS + 8.f, ty * TS + 14.f);  // an old stump
                    else if (r.chance(0.007f)) rockProp(r.chance(0.6f) ? P_ROCK0 : P_ROCK1, tx, ty);
                    else if (r.chance(0.011f)) addDecor(6 + r.irange(0, 1), tx * TS + 8.f, ty * TS + 15.f);
                    else if (r.chance(0.085f)) {
                        const float roll = r.f();
                        int dk = -1;  // -1: nothing here (the forest floors keep clean: only an odd mushroom or stone)
                        if (t == T_LEAF || t == T_NEEDLES) dk = roll < 0.10f ? D_MUSHROOM : roll < 0.16f ? D_PEBBLES : -1;
                        else if (t == T_ROOTS) dk = roll < 0.22f ? D_MUSHROOM : roll < 0.40f ? D_PEBBLES : -1;
                        else if (t == T_MOSS) dk = roll < 0.30f ? D_MUSHROOM : roll < 0.85f ? D_TUFT : D_PEBBLES;
                        else dk = roll < 0.3f ? r.irange(0, 2) : roll < 0.82f ? 3 : roll < 0.9f ? (t == T_DARK ? 4 : 3) : 5;
                        if (dk >= 0) addDecor(dk, dx(), dy());
                    }
                    continue;
                }
                switch (map) {
                    case B_DESERT: {
                        float pc = t == T_SAND ? 0.011f : (t == T_DUNE ? 0.004f : 0.f);
                        if (r.chance(pc)) {
                            tileProp(r.chance(0.55f) ? P_CACTUS0 : P_CACTUS1, tx, ty);
                            break;
                        }
                        if (r.chance(t == T_STONE ? 0.035f : 0.008f)) {
                            const float rr = r.f();  // tiny (hand-mined), small, and now the odd large desert rock
                            rockProp(rr < 0.45f ? P_SANDROCK0 : rr < 0.85f ? P_SANDROCK1 : P_SANDBOULDER, tx, ty);
                            break;
                        }
                        if (r.chance(0.006f)) {  // decor rocks: scenery only, cannot be mined
                            addDecor(D_DROCK0 + r.irange(0, 3), dx(), dy());
                            break;
                        }
                        if (r.chance(0.035f)) addDecor(D_DRYBUSH, dx(), dy());
                        else if (r.chance(0.05f)) addDecor(D_SANDTUFT, dx(), dy());
                        else if (r.chance(0.0035f)) addDecor(D_SKULL, dx(), dy());
                        else if (r.chance(0.012f)) addDecor(D_PEBBLES, dx(), dy());
                        break;
                    }
                    case B_SWAMP: {
                        bool nearWater = tile[ty][tx - 1] == T_MURK || tile[ty][tx + 1] == T_MURK || tile[ty - 1][tx] == T_MURK || tile[ty + 1][tx] == T_MURK ||
                                         tile[ty][tx - 1] == T_BOG || tile[ty][tx + 1] == T_BOG || tile[ty - 1][tx] == T_BOG || tile[ty + 1][tx] == T_BOG;
                        if (t == T_MURK) {
                            if (r.chance(0.012f)) tileProp(P_DEADTREE1, tx, ty);
                            else if (r.chance(0.16f)) addDecor(r.chance(0.35f) ? D_REEDS1 : D_REEDS0, fx + r.range(3, 13), fy + r.range(8, 15));
                            else if (r.chance(0.07f)) addDecor(D_LILY, dx(), dy());
                            break;
                        }
                        if (r.chance(t == T_DARK ? 0.07f : 0.04f)) {
                            tileProp(r.chance(0.5f) ? P_DEADTREE0 : P_DEADTREE1, tx, ty);
                            break;
                        }
                        if (r.chance(0.004f)) {
                            props.push_back({P_LOG, fx + 8.f, fy + 14.f, 0});
                            solid[ty][tx] = true;
                            break;
                        }
                        if (t == T_DARK && r.chance(0.06f)) addDecor(D_BUSH0 + r.irange(0, 1), fx + 8.f, fy + 15.f);
                        else if (nearWater && r.chance(0.22f)) addDecor(r.chance(0.3f) ? D_REEDS1 : D_REEDS0, fx + r.range(3, 13), fy + r.range(8, 15));
                        else if (r.chance(0.035f)) addDecor(D_REEDS0, fx + r.range(3, 13), fy + r.range(8, 15));
                        else if (t == T_MUD && r.chance(0.025f)) addDecor(D_BUBBLES, dx(), dy());
                        else if (t == T_DARK && r.chance(0.035f)) addDecor(D_MUSHROOM, dx(), dy());
                        else if (t == T_DARK && r.chance(0.09f)) addDecor(D_TUFT, dx(), dy());
                        break;
                    }
                    case B_SNOW: {
                        if (t == T_ICE) break;
                        float ff = fbm(tx * 0.07f, ty * 0.07f, S + 303);
                        if (sv2) {
                            float qx = (float)tx, qy = (float)ty;
                            sZWarp.DomainWarp(qx, qy);
                            ff = fn01(sTree, qx, qy);
                        }
                        const SnowZ z = snowZone(tx, ty);
                        // dense forest: thick stands; open ground: a few lone trees; rocky outcrops: almost none
                        float pTree = (0.012f + 0.30f * cl((ff - 0.5f) / 0.2f, 0, 1)) * cl(1.f + (-0.8f + 1.3f * z.forest - 0.45f * z.rock), 0.1f, 3.f) + 0.12f * z.forest;
                        pTree = cl(pTree, 0.f, 0.40f);
                        if (r.chance(pTree)) {
                            tileProp(r.chance(0.82f) ? P_SNOWPINE : P_BARETREE, tx, ty);
                            break;
                        }
                        if (r.chance(0.007f + (0.045f * z.rock - 0.004f * z.forest))) {
                            // the rocky outcrops grow bigger rocks: which size is decided by a hash of the tile (not the shared random stream), so every
                            // other thing in the snowfield stays where it was for an existing seed. Small and large rocks come with a few pebble piles.
                            const float u = (hash2(tx, ty, S + 6101u) & 0xFFFF) / 65535.f;
                            const float pLarge = 0.32f * z.rock * z.rock, pSmall = 0.55f * z.rock;
                            const int kind = u < pLarge ? P_BOULDER : u < pLarge + pSmall ? P_ROCK1 : P_SNOWROCK;
                            rockProp(kind, tx, ty);
                            if (kind != P_SNOWROCK) {
                                const int piles = kind == P_BOULDER ? 3 : 2;
                                for (int k = 0; k < piles; k++) {
                                    const uint32_t h2 = hash2(tx, ty, S + 6110u + (uint32_t)k);
                                    const float a = (h2 & 0xFFFF) / 65535.f * 6.28f, d = 14.f + ((h2 >> 16) & 0xFF) / 255.f * 14.f;
                                    const int px = tx + (int)floorf((tx * (float)TS + 8.f + cosf(a) * d) / TS) - tx, py = ty + (int)floorf((ty * (float)TS + 12.f + sinf(a) * d * 0.6f) / TS) - ty;
                                    if (inMap(px, py) && !solid[py][px] && tile[py][px] != T_ICE && tile[py][px] != T_CLIFF)
                                        decor.push_back({D_PEBBLES, tx * (float)TS + 8.f + cosf(a) * d, ty * (float)TS + 12.f + sinf(a) * d * 0.6f, 0.f});
                                }
                            }
                            break;
                        }
                        if (r.chance(0.0025f + 0.004f * z.rock)) {
                            tileProp(P_CRYSTAL, tx, ty);
                            break;
                        }
                        if (r.chance(0.06f + (0.09f * z.slope - 0.03f * z.forest))) addDecor(D_DRIFT, dx(), dy());
                        else if (r.chance(0.05f + 0.06f * z.tundra)) addDecor(D_FROSTTUFT, dx(), dy());
                        else if (r.chance(0.008f)) addDecor(D_PEBBLES, dx(), dy());
                        // snow bushes: decided by their own per-tile hash and pushed directly, so they take nothing from the shared random
                        // stream and every other thing in the snowfield stays exactly where it was for an existing seed
                        else if ((hash2(tx, ty, S + 5101u) & 0xFFFF) < (uint32_t)((0.009f + 0.012f * z.tundra) * 65536.f))
                            decor.push_back({D_BUSH0 + (int)(hash2(tx, ty, S + 5102u) & 1u), fx + 8.f, fy + 15.f, (hash2(tx, ty, S + 5103u) & 0xFFFF) / 65535.f * 6.28f});
                        break;
                    }
                    default: {  // mountains
                        float ff = fbm(tx * 0.06f, ty * 0.06f, S + 405);
                        bool meadow = t != T_STONE;
                        float pPine = meadow ? 0.07f + 0.15f * cl((ff - 0.5f) / 0.2f, 0, 1) : (ff > 0.55f ? 0.03f : 0.004f);
                        if (r.chance(pPine)) {
                            tileProp(P_PINE, tx, ty);
                            break;
                        }
                        if (!meadow && r.chance(0.011f)) {
                            rockProp(P_BOULDER, tx, ty);
                            break;
                        }
                        if (!meadow && r.chance(0.06f)) addDecor(D_GRAVEL, dx(), dy());
                        else if (r.chance(0.045f)) addDecor(D_ALPINE, dx(), dy());
                        else if (meadow && r.chance(0.08f)) addDecor(D_TUFT, dx(), dy());
                        else if (meadow && r.chance(0.04f)) addDecor(r.irange(0, 2), dx(), dy());
                        // alpine cushions on the meadow, dead twig clumps on the scree: their own per-tile hash, pushed directly, so the shared
                        // random stream (and with it every other thing on the map) is unchanged for an existing seed
                        else if ((hash2(tx, ty, S + 5201u) & 0xFFFF) < (uint32_t)((meadow ? 0.012f : 0.007f) * 65536.f))
                            decor.push_back({D_BUSH0 + (meadow ? 0 : 1), fx + 8.f, fy + 15.f, (hash2(tx, ty, S + 5203u) & 0xFFFF) / 65535.f * 6.28f});
                        break;
                    }
                }
            }

    std::vector<std::vector<int>> platComps;  // the snow plateaus (plat == 1), one list of tile ids each, and how deep inside its top each tile lies (5d; 7 uses them too)
    std::vector<uint8_t> platDep;
    // ------------------------------------------------------------ 5b. snowfield props: signposts, lamps, a yard fence and snowmen at the landmarks, and old stumps in the drifts.
    // All of it uses the tile hash and no random numbers, so everything else on the map is placed exactly as before.
    if (map == B_SNOW) {
        auto propNear = [&](float x, float y, float d) {
            for (const Prop& p : props)
                if (fabsf(p.x - x) < d && fabsf(p.y - y) < d) return true;
            return false;
        };
        auto freeAt = [&](int x, int y) {
            return inMap(x, y) && !inRing(x, y) && K(x, y) != 1 && !solid[y][x] && (tile[y][x] == T_SNOW || tile[y][x] == T_SNOWP) && !propNear(x * TS + 8.f, y * TS + 15.f, 14.f);
        };
        auto put = [&](int kind, int tx, int ty, float ph, int search) -> bool {  // the free tile nearest (tx,ty), within `search` tiles
            for (int rad = 0; rad <= search; rad++)
                for (int dy = -rad; dy <= rad; dy++)
                    for (int dx = -rad; dx <= rad; dx++) {
                        if (std::max(abs(dx), abs(dy)) != rad || !freeAt(tx + dx, ty + dy)) continue;
                        const int x = tx + dx, y = ty + dy;
                        props.push_back({kind, x * TS + 8.f, y * TS + 15.f, ph});
                        solid[y][x] = true;
                        if (K(x, y) < 3) K(x, y) = 3;
                        if (kind == P_SNOWLAMP) lanterns.push_back({x * TS + 8.f, y * TS + 15.f - 24.f, 0.75f});
                        return true;
                    }
            return false;
        };
        const int hx = MD.hubTx, hy = MD.hubTy, cx = cairnTx, cy = cairnTy, fx = fortTx, fy = fortTy;
        put(P_SNOWSIGN, hx + 4, hy + 3, 0.f, 2);
        put(P_SNOWSIGN, 62, 48, 1.f, 3);
        put(P_SNOWSIGN, cx + 3, cy + 3, 2.f, 2);
        put(P_SNOWSIGN, fx + 11, fy + 10, 3.f, 2);
        put(P_SNOWLAMP, hx - 5, hy + 3, 0.f, 2);
        put(P_SNOWLAMP, hx + 7, hy + 2, 0.f, 2);
        put(P_SNOWLAMP, fx - 11, fy + 10, 0.f, 2);
        put(P_SNOWLAMP, fx + 12, fy + 9, 0.f, 2);
        for (int x = hx + 2; x <= hx + 8; x++)  // a yard fence east of the hut, with a gap in the middle
            if (x != hx + 5) put(P_SNOWFENCE, x, hy + 6, 0.f, 0);
        put(P_FISHRACK, hx - 4, hy, 0.f, 2);   // the fisher's drying rack and a stack of firewood by the hut
        put(P_FIREWOOD, hx + 3, hy - 1, 0.f, 2);
        {   // ice-fishing holes cut in the hut's pond
            const int hole[4][2] = {{-11, 4}, {-8, 3}, {-9, 6}, {-12, 6}};
            for (const auto& h : hole) {
                const int x = hx + h[0], y = hy + h[1];
                if (inMap(x, y) && tile[y][x] == T_ICE) decor.push_back({D_FISHHOLE, x * TS + 8.f, y * TS + 8.f, 0.f});
            }
        }
        put(P_SNOWMAN, hx + 6, hy + 4, 0.f, 2);
        put(P_SNOWMAN, 57, 49, 1.f, 3);
        put(P_SNOWMAN, cx - 5, cy + 3, 2.f, 2);
        put(P_SNOWMAN, fx - 13, fy + 11, 3.f, 2);
        for (int ty = 0; ty < MH; ty++)
            for (int tx = 0; tx < MW; tx++)
                if ((hash2(tx, ty, S + 6101u) & 0xFFFF) < (uint32_t)(0.0010f * 65536.f) && K(tx, ty) == 0 && freeAt(tx, ty))
                    decor.push_back({D_STUMP, tx * TS + 8.f, ty * TS + 14.f, (hash2(tx, ty, S + 6103u) & 0xFFFF) / 65535.f * 6.28f});
        // ridge tidy-up. The ground stage lays the ridges as a band a few tiles wide, which holds together; here it is thinned to a line one tile
        // wide (Zhang-Suen thinning, which keeps every piece connected), so the ridges come out long and continuous instead of a dotted contour.
        // A piece (cliff tiles touching, diagonals included) shorter than 16 tiles is then levelled, and a long ridge gets a pass cut across it
        // every ~32 tiles (3 tiles wide, always clear of the ends). (Not with plateaus: their faces are made before this stage and must stay two tiles tall.)
        if (!plateaus) {
            std::vector<char> seen((size_t)MW * MH, 0);
            const int nb[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
            auto flood = [&](int sx, int sy, std::vector<int>& dist, std::vector<int>& cells) {  // breadth-first along the piece: cells in order, dist = steps from the start
                size_t head = cells.size();
                cells.push_back(sy * MW + sx);
                dist.push_back(0);
                while (head < cells.size()) {
                    const int cx0 = cells[head] % MW, cy0 = cells[head] / MW, d0 = dist[head];
                    head++;
                    for (auto& o : nb) {
                        const int nx = cx0 + o[0], ny = cy0 + o[1];
                        if (nx < 0 || ny < 0 || nx >= MW || ny >= MH || tile[ny][nx] != T_CLIFF || seen[ny * MW + nx]) continue;
                        seen[ny * MW + nx] = 1;
                        cells.push_back(ny * MW + nx);
                        dist.push_back(d0 + 1);
                    }
                }
            };
            auto level = [&](int x, int y) {  // back to the snow around it
                uint8_t g = T_SNOW;
                for (auto& o : nb) {
                    const int nx = x + o[0], ny = y + o[1];
                    if (nx >= 0 && ny >= 0 && nx < MW && ny < MH && (tile[ny][nx] == T_SNOWP || tile[ny][nx] == T_SNOW)) { g = tile[ny][nx]; break; }
                }
                tile[y][x] = g;
            };
            {
                std::vector<char> m((size_t)MW * MH, 0);
                for (int y = 0; y < MH; y++)
                    for (int x = 0; x < MW; x++) m[y * MW + x] = tile[y][x] == T_CLIFF;
                auto at = [&](int x, int y) { return (int)m[y * MW + x]; };
                for (bool changed = true; changed;) {
                    changed = false;
                    for (int step = 0; step < 2; step++) {
                        std::vector<int> del;
                        for (int y = 1; y < MH - 1; y++)
                            for (int x = 1; x < MW - 1; x++) {
                                if (!at(x, y)) continue;
                                const int p[8] = {at(x, y - 1), at(x + 1, y - 1), at(x + 1, y), at(x + 1, y + 1), at(x, y + 1), at(x - 1, y + 1), at(x - 1, y), at(x - 1, y - 1)};
                                int b = 0, a = 0;
                                for (int i = 0; i < 8; i++) { b += p[i]; a += !p[i] && p[(i + 1) % 8]; }
                                if (b < 2 || b > 6 || a != 1) continue;
                                if (step == 0 ? (p[0] && p[2] && p[4]) || (p[2] && p[4] && p[6]) : (p[0] && p[2] && p[6]) || (p[0] && p[4] && p[6])) continue;
                                del.push_back(y * MW + x);
                            }
                        for (int d : del) m[d] = 0;
                        if (!del.empty()) changed = true;
                    }
                }
                for (int y = 0; y < MH; y++)
                    for (int x = 0; x < MW; x++)
                        if (tile[y][x] == T_CLIFF && !m[y * MW + x]) level(x, y);
            }
            // pockets: a ridge that closes on itself shuts a patch of ground off. Wherever such a pocket is cut off by a ridge one tile thick, that
            // tile is levelled, so the ground inside joins the rest (the big halves of the map are never merged this way, only pockets).
            {
                auto blocked = [&](int x, int y) { return inRing(x, y) || solid[y][x] || tileSolid(tile[y][x]); };  // the edge wall is not built yet: its ring counts as wall
                std::vector<int> reg((size_t)MW * MH, -1), par, sz;
                auto find = [&](int a) { while (par[a] != a) a = par[a] = par[par[a]]; return a; };
                for (int y = 0; y < MH; y++)
                    for (int x = 0; x < MW; x++) {
                        if (blocked(x, y) || reg[y * MW + x] >= 0) continue;
                        const int id = (int)par.size();
                        par.push_back(id);
                        sz.push_back(0);
                        std::vector<int> st{y * MW + x};
                        reg[y * MW + x] = id;
                        while (!st.empty()) {
                            const int c = st.back();
                            st.pop_back();
                            sz[id]++;
                            const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                            for (auto& o : d4) {
                                const int nx = c % MW + o[0], ny = c / MW + o[1];
                                if (nx < 0 || ny < 0 || nx >= MW || ny >= MH || blocked(nx, ny) || reg[ny * MW + nx] >= 0) continue;
                                reg[ny * MW + nx] = id;
                                st.push_back(ny * MW + nx);
                            }
                        }
                    }
                for (int y = 1; y < MH - 1; y++)
                    for (int x = 1; x < MW - 1; x++) {
                        if (tile[y][x] != T_CLIFF) continue;
                        // the ground around it (any two of its four neighbours, so a staircase of ridge can be cut as well as a straight one)
                        const int nbr[4] = {reg[y * MW + x - 1], reg[y * MW + x + 1], reg[(y - 1) * MW + x], reg[(y + 1) * MW + x]};
                        bool cut = false;
                        for (int u = 0; u < 4 && !cut; u++)
                            for (int v = u + 1; v < 4 && !cut; v++) {
                                if (nbr[u] < 0 || nbr[v] < 0) continue;
                                const int a = find(nbr[u]), b = find(nbr[v]);
                                if (a == b || (sz[a] >= 30000 && sz[b] >= 30000)) continue;
                                par[b] = a;
                                sz[a] += sz[b] + 1;
                                cut = true;
                            }
                        if (!cut) continue;
                        tile[y][x] = (tile[y][x - 1] == T_SNOWP || tile[y][x + 1] == T_SNOWP) ? T_SNOWP : T_SNOW;
                    }
            }
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++) {
                    if (tile[y][x] != T_CLIFF || seen[y * MW + x]) continue;
                    std::vector<int> cells, dist;
                    seen[y * MW + x] = 1;
                    flood(x, y, dist, cells);
                    if (cells.size() < 16) { for (int c : cells) level(c % MW, c / MW); continue; }
                    // the far end of the piece, then the distance along it from there
                    const int far = cells.back();
                    std::vector<int> cells2, dist2;
                    for (int c : cells) seen[c] = 0;
                    seen[far] = 1;
                    flood(far % MW, far / MW, dist2, cells2);
                    const int maxd = dist2.back(), nPass = (maxd - 14) / 32;
                    for (int k = 0; k < nPass; k++) {
                        const int at = 14 + (int)((k + 0.5f) * (maxd - 28) / nPass) + (int)(hash2(cells[0] % MW, cells[0] / MW + k, S + 6451u) % 7u) - 3;
                        for (size_t i = 0; i < cells2.size(); i++)
                            if (dist2[i] >= at && dist2[i] < at + 3) level(cells2[i] % MW, cells2[i] / MW);
                    }
                }
        }
        // frozen features: little ponds and winding streams of ice, with clusters of ice crystals on their banks and in the rocky country. Ice is
        // walkable (and slippery); a trail or plaza that a stream runs into simply breaks it there, and a pond that would be cut by one is not made.
        // Everything comes from the tile hash and no random numbers.
        {
            auto icable = [&](int x, int y) { return inMap(x, y) && !inRing(x, y) && plat[y][x] == 0 && x > 4 && y > 4 && x < MW - 5 && y < MH - 5 && K(x, y) == 0 && (tile[y][x] == T_SNOW || tile[y][x] == T_SNOWP); };
            auto freeze = [&](int x, int y) {
                tile[y][x] = T_ICE;
                solid[y][x] = false;
                K(x, y) = 3;
                props.erase(std::remove_if(props.begin(), props.end(), [&](const Prop& p) { return (int)(p.x / TS) == x && (int)(p.y / TS) == y; }), props.end());
                decor.erase(std::remove_if(decor.begin(), decor.end(), [&](const Decor& d) { return (int)(d.x / TS) == x && (int)(d.y / TS) == y; }), decor.end());
            };
            // a cluster of 3-4 crystals around a tile (bank snow only, nothing on a trail or in a plaza)
            auto crystals = [&](int bx, int by, uint32_t h) {
                const int n = 3 + (int)(h & 1u);
                for (int m = 0; m < n; m++) {
                    const uint32_t g = hash2(bx * 7 + m, by, h + 11u);
                    const int tx = bx + (int)(g & 3u) - 1 + (m ? (int)(g >> 6 & 1u) : 0), ty = by + (int)(g >> 3 & 3u) - 1;
                    if (!inMap(tx, ty) || inRing(tx, ty) || tx < 4 || ty < 4 || tx >= MW - 5 || ty >= MH - 5 || K(tx, ty) != 0 || solid[ty][tx] || (tile[ty][tx] != T_SNOW && tile[ty][tx] != T_SNOWP)) continue;
                    props.push_back({P_CRYSTAL, tx * TS + 8.f + (float)(g >> 8 & 3u) - 1.f, ty * TS + 15.f, (g & 0xFFFF) / 65535.f * 6.28f});
                    solid[ty][tx] = true;
                }
            };
            // the big lakes (see the ground stage) and the landmarks: ponds keep well clear of them
            const float lkc[5][4] = {{60, 40, 11, 6}, {190, 50, 9, 5}, {30, 110, 8, 4.5f}, {160, 20, 7, 4}, {200, 140, 10, 5.5f}};
            const float keep[4][3] = {{(float)hx, (float)hy, 17.f}, {(float)fx, (float)fy, 30.f}, {(float)cx, (float)cy, 12.f}, {60.f, 47.f, 8.f}};
            // ponds: one candidate per 34 x 28 tile cell, about a third of them made, the open country preferred
            for (int gy = 0; gy < 6; gy++)
                for (int gx = 0; gx < 7; gx++) {
                    const uint32_t h = hash2(gx, gy, S + 6501u);
                    if (h % 10u >= 6u) continue;
                    const float pcx = gx * 34.f + 17.f + (float)(h >> 4 & 15u) - 8.f, pcy = gy * 28.f + 14.f + (float)(h >> 8 & 15u) - 8.f;
                    const float prx = 2.7f + (float)(h >> 12 & 7u) * 0.3f, pry = prx * (0.62f + (float)(h >> 16 & 3u) * 0.06f);
                    bool clear = true;
                    for (auto& l : lkc) clear = clear && (pcx - l[0]) * (pcx - l[0]) / ((l[2] + prx + 4.f) * (l[2] + prx + 4.f)) + (pcy - l[1]) * (pcy - l[1]) / ((l[3] + pry + 4.f) * (l[3] + pry + 4.f)) > 1.f;
                    for (auto& l : keep) clear = clear && hypotf(pcx - l[0], pcy - l[1]) > l[2] + prx;
                    if (!clear || pcx < 12 || pcy < 12 || pcx > MW - 12 || pcy > MH - 12) continue;
                    const SnowZ z = snowZone((int)pcx, (int)pcy);
                    if (z.forest > 0.5f) continue;
                    std::vector<int> cells;
                    int bad = 0;
                    for (int y = (int)(pcy - pry - 2); y <= (int)(pcy + pry + 2); y++)
                        for (int x = (int)(pcx - prx - 2); x <= (int)(pcx + prx + 2); x++) {
                            const float dx = (x - pcx) / prx, dy = (y - pcy) / pry;
                            if (dx * dx + dy * dy + (fbm(x * 0.3f, y * 0.3f, S + 6502u) - 0.5f) * 0.6f >= 1.f) continue;
                            if (icable(x, y)) cells.push_back(y * MW + x); else bad++;
                        }
                    if (cells.size() < 8 || bad * 7 > (int)cells.size()) continue;  // not enough room, or a trail or ridge would cut it
                    for (int c : cells) freeze(c % MW, c / MW);
                    for (int k = 0; k < 2; k++)  // crystals on the bank of some ponds
                        if ((h >> (20 + k) & 1u) && h % 3u) {
                            const float a = (float)(hash2(gx, gy, S + 6503u + k) & 0xFFFF) / 65535.f * 6.2832f;
                            crystals((int)lroundf(pcx + cosf(a) * (prx + 1.6f)), (int)lroundf(pcy + sinf(a) * (pry + 1.6f)), h + (uint32_t)k * 31u);
                        }
                }
            // streams: a winding band of ice two tiles wide, leaving each lake and a few starting in the open
            const int nStream = 5 + 3;
            for (int i = 0; i < nStream; i++) {
                const uint32_t h = hash2(i, 77, S + 6511u);
                float sx, sy, ang;
                if (i < 5) {  // an outflow from lake i, heading away from it
                    const float a = (float)(h & 0xFFFF) / 65535.f * 6.2832f;
                    sx = lkc[i][0] + cosf(a) * lkc[i][2] * 0.9f;
                    sy = lkc[i][1] + sinf(a) * lkc[i][3] * 0.9f;
                    ang = a + ((float)(h >> 16 & 0xFF) / 255.f - 0.5f) * 0.6f;
                } else {
                    sx = 25.f + (float)(h % 190u);
                    sy = 25.f + (float)(h >> 8 & 0x7Fu) * 0.9f;
                    ang = (float)(h >> 16 & 0xFFFF) / 65535.f * 6.2832f;
                }
                const float len = 36.f + (float)(h >> 4 & 31u), amp = 2.5f + (float)(h >> 9 & 7u) * 0.5f, fr = 0.10f + (float)(h >> 12 & 7u) * 0.01f, ph = (float)(h >> 15 & 63u) * 0.1f;
                const float dxn = cosf(ang), dyn = sinf(ang), nx = -dyn, ny = dxn;
                int made = 0;
                for (float t = -4.f; t < len; t += 0.5f) {
                    const float taper = cl(std::min((t + 4.f) / 6.f, (len - t) / 6.f), 0.f, 1.f);
                    const float rad = (0.75f + 0.35f * sinf(t * 0.13f + ph)) * (0.4f + 0.6f * taper);
                    const float px = sx + dxn * t + nx * amp * sinf(t * fr + ph), py = sy + dyn * t + ny * amp * sinf(t * fr + ph);
                    for (int y = (int)floorf(py - rad - 0.5f); y <= (int)ceilf(py + rad + 0.5f); y++)
                        for (int x = (int)floorf(px - rad - 0.5f); x <= (int)ceilf(px + rad + 0.5f); x++) {
                            if ((x + 0.5f - px) * (x + 0.5f - px) + (y + 0.5f - py) * (y + 0.5f - py) > (rad + 0.45f) * (rad + 0.45f) || !icable(x, y)) continue;
                            freeze(x, y);
                            made++;
                        }
                    // a crystal cluster on the bank, now and then
                    if (made > 0 && (hash2(i * 131 + (int)(t * 2.f), 5, S + 6521u) % 130u) == 0)
                        crystals((int)lroundf(px + nx * (rad + 2.4f)), (int)lroundf(py + ny * (rad + 2.4f)), h + (uint32_t)(t * 2.f));
                }
            }
            // crystal clusters in the rocky country: one candidate per 24 x 24 cell
            for (int gy = 0; gy < 7; gy++)
                for (int gx = 0; gx < 10; gx++) {
                    const uint32_t h = hash2(gx, gy, S + 6531u);
                    const int bx = gx * 24 + 4 + (int)(h % 16u), by = gy * 24 + 4 + (int)(h >> 5 & 15u);
                    if (!inMap(bx, by) || snowZone(bx, by).rock < 0.45f || (h >> 10 & 3u) != 0) continue;
                    crystals(bx, by, h);
                }
        }
        // landmark framing: each landmark sits in a loose ring of rock ridge, tree line and rock clusters, open on the side its trail comes from.
        // Nothing is put on a trail or in a cleared plaza (keep mask), so the approach stays clear and the ring is simply cut where a trail
        // crosses it. Everything comes from the tile hash and no random numbers.
        {
            struct Frame { float cx, cy, rx, ry, ax, ay, open; int id; float gate; };  // centre, ridge radii (tiles), approach direction, half-angle of the open front, distance of the gateway outcrops
            const Frame frames[] = {
                {(float)hx, (float)hy, 13.f, 10.f, 0.f, 1.f, 55.f, 1, 11.f},     // the hut: its trail comes up from the south gate
                {60.f, 40.f, 17.f, 11.f, 0.59f, 0.81f, 65.f, 2, 0.f},      // the frozen lake: the spur arrives from the south-east
                {(float)cx, (float)cy, 9.f, 7.5f, -0.72f, 0.70f, 60.f, 3, 7.f},  // the watch cairn: from the south-west
                {(float)fx, (float)fy, 25.f, 22.f, 0.f, 1.f, 62.f, 4, 0.f},      // the fortress, whose gate faces south
            };
            const float TAU = 6.2832f;
            auto snowAt = [&](int x, int y) { return inMap(x, y) && !inRing(x, y) && K(x, y) == 0 && !solid[y][x] && (tile[y][x] == T_SNOW || tile[y][x] == T_SNOWP); };
            auto frontOff = [&](const Frame& f, float th) { return acosf(cl(cosf(th) * f.ax + sinf(th) * f.ay, -1.f, 1.f)) * 57.2958f; };  // degrees from the approach
            for (const Frame& f : frames) {
                const float circ = TAU * 0.5f * (f.rx + f.ry);
                const uint32_t sd = S + 6401u + (uint32_t)f.id * 17u;
                // the ridge: segments of rock with gaps between them, the radius wandering with a slow noise
                const int steps = (int)(circ * 2.f), nSeg = std::max(4, (int)(circ / 7.f));
                for (int i = 0; i < steps; i++) {
                    const float th = i * TAU / steps, c = cosf(th), sn = sinf(th);
                    if (frontOff(f, th) < f.open) continue;
                    if ((hash2(i * nSeg / steps, f.id, sd) & 3u) == 0) continue;
                    const float wob = 1.f + (fbm(c * 2.f + f.id * 9.f, sn * 2.f + 4.f, sd + 1u) - 0.5f) * 0.34f;
                    const int tx = (int)lroundf(f.cx + c * f.rx * wob), ty = (int)lroundf(f.cy + sn * f.ry * wob);
                    if (!snowAt(tx, ty)) continue;
                    tile[ty][tx] = T_CLIFF;
                    solid[ty][tx] = true;
                    K(tx, ty) = 3;
                    props.erase(std::remove_if(props.begin(), props.end(), [&](const Prop& p) { return (int)(p.x / TS) == tx && (int)(p.y / TS) == ty; }), props.end());
                    decor.erase(std::remove_if(decor.begin(), decor.end(), [&](const Decor& d) { return (int)(d.x / TS) == tx && (int)(d.y / TS) == ty; }), decor.end());
                }
                // the tree line: two staggered rows inside the ridge, thinning towards the open front
                for (int row = 0; row < 2; row++) {
                    const float k = row ? 0.70f : 0.82f;
                    const int nT = (int)(circ * k / 2.3f);
                    for (int j = 0; j < nT; j++) {
                        const uint32_t h = hash2(j, f.id * 4 + row, sd + 2u);
                        const float th = (j + (row ? 0.5f : 0.f) + (h & 0xFF) / 255.f * 0.6f) * TAU / nT;
                        if (frontOff(f, th) < f.open + 8.f || (h >> 8 & 7u) == 0) continue;
                        const float jr = 1.f + ((h >> 12 & 0xFF) / 255.f - 0.5f) * 0.16f;
                        const int tx = (int)lroundf(f.cx + cosf(th) * f.rx * k * jr), ty = (int)lroundf(f.cy + sinf(th) * f.ry * k * jr);
                        if (!snowAt(tx, ty) || propNear(tx * TS + 8.f, ty * TS + 15.f, 14.f)) continue;
                        props.push_back({(h >> 20 & 0xFF) < 205 ? P_SNOWPINE : P_BARETREE, tx * TS + 8.f, ty * TS + 15.f, (h & 0xFFFF) / 65535.f * 6.28f});
                        solid[ty][tx] = true;
                        K(tx, ty) = 3;
                    }
                }
                // a gateway: a pair of outcrops (a short ridge, a rock and two pines each) flanking the trail some way out, so the way in reads as a pass
                if (f.gate > 0.f) {
                    const float gx = f.cx + f.ax * f.gate, gy = f.cy + f.ay * f.gate;
                    for (int side = -1; side <= 1; side += 2) {
                        const float ox = gx - f.ay * side * 6.f, oy = gy + f.ax * side * 6.f;  // 6 tiles to either side of the trail's line
                        for (int t = -2; t <= 2; t++) {  // the ridge runs along the approach
                            const uint32_t h = hash2(t + 9, f.id * 2 + (side > 0), sd + 5u);
                            const int tx = (int)lroundf(ox + f.ax * t + (float)(h & 1u) * 0.f), ty = (int)lroundf(oy + f.ay * t);
                            if (!snowAt(tx, ty) || (h >> 4 & 7u) == 0) continue;
                            tile[ty][tx] = T_CLIFF;
                            solid[ty][tx] = true;
                            K(tx, ty) = 3;
                            props.erase(std::remove_if(props.begin(), props.end(), [&](const Prop& p) { return (int)(p.x / TS) == tx && (int)(p.y / TS) == ty; }), props.end());
                            decor.erase(std::remove_if(decor.begin(), decor.end(), [&](const Decor& d) { return (int)(d.x / TS) == tx && (int)(d.y / TS) == ty; }), decor.end());
                        }
                        for (int m = 0; m < 4; m++) {  // two pines and two rocks on the outer side of the ridge
                            const uint32_t h = hash2(m, f.id * 2 + (side > 0), sd + 6u);
                            const float off = (m < 2 ? 2.2f : 1.2f) * side, al = (float)(h & 3u) - 1.5f;
                            const int tx = (int)lroundf(ox - f.ay * off + f.ax * al), ty = (int)lroundf(oy + f.ax * off + f.ay * al);
                            if (!snowAt(tx, ty) || propNear(tx * TS + 8.f, ty * TS + 15.f, 12.f)) continue;
                            props.push_back({m < 2 ? P_SNOWPINE : P_SNOWROCK, tx * TS + 8.f, ty * TS + (m < 2 ? 15.f : 14.f), (h >> 8 & 0xFF) / 255.f * 6.28f});
                            solid[ty][tx] = true;
                            K(tx, ty) = 3;
                        }
                    }
                }
                // a few rock clusters on the ridge's inner side
                for (int q = 0; q < 4; q++) {
                    const float th = (hash2(q, f.id, sd + 3u) & 0xFFFF) / 65535.f * TAU;
                    if (frontOff(f, th) < f.open) continue;
                    const int bx = (int)lroundf(f.cx + cosf(th) * f.rx * 0.9f), by = (int)lroundf(f.cy + sinf(th) * f.ry * 0.9f);
                    for (int m = 0; m < 3; m++) {
                        const uint32_t h = hash2(q * 3 + m, f.id, sd + 4u);
                        const int tx = bx + (int)(h & 3u) - 1 + (m ? (int)(h >> 4 & 3u) - 1 : 0), ty = by + (int)(h >> 8 & 3u) - 1;
                        if (!snowAt(tx, ty) || propNear(tx * TS + 8.f, ty * TS + 14.f, 11.f)) continue;
                        props.push_back({P_SNOWROCK, tx * TS + 8.f, ty * TS + 14.f, 0.f});
                        solid[ty][tx] = true;
                        K(tx, ty) = 3;
                    }
                }
            }
        }
        // ground detail, from the tile hash alone: frost cracks on the packed snow, paw tracks along the trails, and rocks poking out of the drifts
        {
            auto groundOk = [&](int x, int y) {
                return inMap(x, y) && !inRing(x, y) && !solid[y][x] && (tile[y][x] == T_SNOW || tile[y][x] == T_SNOWP) && !propNear(x * TS + 8.f, y * TS + 8.f, 10.f);
            };
            auto roll = [&](int x, int y, uint32_t salt) { return (hash2(x, y, S + salt) & 0xFFFF) / 65535.f; };
            for (int ty = 0; ty < MH; ty++)
                for (int tx = 0; tx < MW; tx++) {
                    if (!groundOk(tx, ty)) continue;
                    const float jx = 3.f + roll(tx, ty, 6212u) * 10.f, jy = 4.f + roll(tx, ty, 6213u) * 8.f, ph = roll(tx, ty, 6214u) * 6.28f;
                    if (K(tx, ty) == 1) {  // on a trail
                        if (roll(tx, ty, 6220u) < 0.16f) decor.push_back({D_TRACKS, tx * TS + jx, ty * TS + jy, ph});
                        continue;
                    }
                    if (K(tx, ty) != 0) continue;
                    const bool packed = tile[ty][tx] == T_SNOWP;
                    const float r0 = roll(tx, ty, 6221u);
                    const SnowZ z = snowZone(tx, ty);
                    const float cr = (packed ? 0.20f : 0.045f) + 0.05f * z.tundra;  // cracks like the open tundra, buried rocks the outcrops
                    if (r0 < cr) decor.push_back({D_FROSTCRACK, tx * TS + jx, ty * TS + jy, ph});
                    else if (r0 < cr + 0.022f + 0.07f * z.rock) decor.push_back({D_BURIEDROCK, tx * TS + jx, ty * TS + jy, ph});
                    else if (r0 < 0.9f && r0 > 0.872f) decor.push_back({D_TRACKS, tx * TS + jx, ty * TS + jy, ph});
                }
        }
        // campsites: a handful, out in the woods - a fire in a ring of stones, a tent beside it and log seats round it. One at most in each of six
        // regions of the map, on the clearest open ground (preferring forest, and a trail within a few tiles), picked from the tile hash alone.
        {
            std::vector<char> tree((size_t)MW * MH, 0);  // tiles held only by a tree trunk: a campsite may clear those away, it makes a clearing in the woods
            for (const Prop& p : props)
                if (p.kind == P_SNOWPINE || p.kind == P_BARETREE) tree[(size_t)((int)(p.y / TS)) * MW + (int)(p.x / TS)] = 1;
            auto openAt = [&](int x, int y) {
                if (!inMap(x, y) || inRing(x, y) || (tile[y][x] != T_SNOW && tile[y][x] != T_SNOWP)) return false;
                return solid[y][x] ? (tree[(size_t)y * MW + x] && K(x, y) != 1 && K(x, y) != 2) : K(x, y) == 0;
            };
            const int lm[4][2] = {{hx, hy}, {62, 48}, {cx, cy}, {fx, fy}};
            std::vector<std::pair<int, int>> camps;
            for (int ry = 0; ry < 2; ry++)
                for (int rx = 0; rx < 3; rx++) {
                    float best = -1.f; int bx = -1, by = -1;
                    for (int t = 0; t < 160; t++) {
                        const int x = rx * 80 + 14 + (int)(hash2(rx * 97 + t, ry, S + 7001u) % 52u), y = ry * 84 + 14 + (int)(hash2(rx * 97 + t, ry, S + 7002u) % 56u);
                        bool ok = true;
                        int trees = 0;
                        for (int dy = -3; dy <= 2 && ok; dy++)
                            for (int dx = -4; dx <= 3 && ok; dx++) { ok = openAt(x + dx, y + dy); trees += ok && solid[y + dy][x + dx]; }
                        for (const auto& l : lm)
                            if (abs(l[0] - x) < 26 && abs(l[1] - y) < 22) ok = false;
                        if (!ok || trees > 9) continue;
                        float sc = snowZone(x, y).forest + (hash2(x, y, S + 7003u) & 255u) / 1024.f;
                        for (int dy = -8; dy <= 8; dy += 2)
                            for (int dx = -8; dx <= 8; dx += 2)
                                if (inMap(x + dx, y + dy) && K(x + dx, y + dy) == 1) { sc += 0.35f; dy = 9; break; }
                        if (sc > best) best = sc, bx = x, by = y;
                    }
                    if (bx >= 0) camps.push_back({bx, by});
                }
            int idx = 0;
            for (const auto& c : camps) {
                const int x = c.first, y = c.second;
                // the clearing: every tree trunk standing in the camp's box goes, and the crowns just south of it (they would hang over the seats)
                props.erase(std::remove_if(props.begin(), props.end(), [&](const Prop& p) {
                    if (p.kind != P_SNOWPINE && p.kind != P_BARETREE) return false;
                    const int ptx = (int)(p.x / TS), pty = (int)(p.y / TS);
                    if (ptx < x - 4 || ptx > x + 3 || pty < y - 3 || pty > y + 4) return false;
                    if (solid[pty][ptx]) { solid[pty][ptx] = false; if (K(ptx, pty) == 3) K(ptx, pty) = 0; }
                    return true;
                }), props.end());
                auto block = [&](int tx, int ty) { solid[ty][tx] = true; K(tx, ty) = 3; };
                // four kinds of camp, dealt out so neighbours differ: 0 the plain camp (fire, tent, three seats), 1 a hunter's camp (a drying rack and
                // firewood instead of the tent), 2 an abandoned camp (a fallen tent and a cold fire ring: no light, no smoke), 3 a supply camp (the
                // plain camp with a crate and a barrel)
                const int variant = (idx + (int)(S & 3u)) & 3;
                if (variant == 2) {
                    props.push_back({P_CAMPFIRE, x * TS + 8.f, y * TS + 13.f, (float)this->camps.size()});  // a cold fire, lit by walking up to it
                    this->camps.push_back({x * TS + 8.f, y * TS + 13.f, false, false});
                    block(x, y);
                    props.push_back({P_SNOWTENTOLD, (float)((x - 2) * TS), (y - 1) * TS + 15.f, 0.f});
                    block(x - 3, y - 1); block(x - 2, y - 1);
                } else {
                    props.push_back({P_SNOWCAMPFIRE, x * TS + 8.f, y * TS + 13.f, (float)(idx * 5)});
                    this->camps.push_back({x * TS + 8.f, y * TS + 13.f, false, true});  // already burning: walking up finds the camp
                    block(x, y);
                    lanterns.push_back({x * TS + 8.f, y * TS + 5.f, 1.3f});
                    if (variant == 1) {
                        props.push_back({P_FISHRACK, (float)((x - 2) * TS + 8), (y - 1) * TS + 15.f, 0.f});  // meat and fish drying
                        block(x - 2, y - 1);
                        props.push_back({P_FIREWOOD, (float)((x - 3) * TS + 8), (y + 1) * TS + 15.f, 0.f});
                        block(x - 3, y + 1);
                    } else {
                        props.push_back({P_SNOWTENT, (float)((x - 2) * TS), (y - 1) * TS + 15.f, 0.f});  // astride two tiles, a little way off to the north-west
                        block(x - 3, y - 1); block(x - 2, y - 1);
                    }
                }
                if (variant == 3) {
                    props.push_back({P_CRATE, (x + 3) * TS + 8.f, (y - 1) * TS + 15.f, 0.f});
                    block(x + 3, y - 1);
                    props.push_back({P_BARREL, (x + 3) * TS + 8.f, (y + 1) * TS + 15.f, 0.f});
                    block(x + 3, y + 1);
                }
                const int seat[3][2] = {{2, 0}, {0, 2}, {1, -2}};
                const int seats = variant == 2 ? 1 : (variant == 1 ? 2 : 3);  // an abandoned camp keeps one log, a hunter's camp two
                for (int si = 0; si < seats; si++) {
                    const int* s = seat[si];
                    props.push_back({P_SNOWLOG, (x + s[0]) * TS + 8.f, (y + s[1]) * TS + 14.f, 0.f});
                    block(x + s[0], y + s[1]);
                }
                decor.erase(std::remove_if(decor.begin(), decor.end(), [&](const Decor& d) { return fabsf(d.x - (x * TS + 8.f)) < 6.f * TS && fabsf(d.y - (y * TS + 8.f)) < 4.f * TS; }), decor.end());
                idx++;
            }
        }
        // ---- 5d. plateau tops. The tops are kept clear of everything while the plateaus are laid (they are walked end to end), so they would be empty rooms; this
        // gives them something, away from the edges where the hunter walks: stones in the bigger ones, a lone tree in the broad ones and, on the biggest, a
        // lookout (a cairn, and a camp: a lit fire with seats or a cold ring). Every choice is the tile hash, so nothing else on the map moves.
        if (plateaus) {
            platDep.assign((size_t)MW * MH, 0);
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++) {
                    if (plat[y][x] != 1) continue;
                    int d = 6;
                    for (int rr = 1; rr <= 5 && d == 6; rr++)
                        for (int dy = -rr; dy <= rr && d == 6; dy++)
                            for (int dx = -rr; dx <= rr; dx++) {
                                if (std::max(abs(dx), abs(dy)) != rr) continue;
                                if (!inMap(x + dx, y + dy) || plat[y + dy][x + dx] != 1) { d = rr; break; }
                            }
                    platDep[(size_t)y * MW + x] = (uint8_t)d;  // 1 = on the edge ... 6 = five tiles or more from it
                }
            std::vector<char> seenC((size_t)MW * MH, 0);
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++) {
                    if (plat[y][x] != 1 || seenC[(size_t)y * MW + x]) continue;
                    std::vector<int> cells{y * MW + x};
                    seenC[(size_t)y * MW + x] = 1;
                    for (size_t h = 0; h < cells.size(); h++) {
                        const int px = cells[h] % MW, py = cells[h] / MW;
                        const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                        for (auto& o : d4) {
                            const int nx = px + o[0], ny = py + o[1];
                            if (!inMap(nx, ny) || plat[ny][nx] != 1 || seenC[(size_t)ny * MW + nx]) continue;
                            seenC[(size_t)ny * MW + nx] = 1;
                            cells.push_back(ny * MW + nx);
                        }
                    }
                    platComps.push_back(cells);
                }
            auto setP = [&](int kind, int tx, int ty, float yoff, float ph) {
                props.push_back({kind, tx * TS + 8.f, ty * TS + yoff, ph});
                solid[ty][tx] = true;
                K(tx, ty) = 3;
            };
            for (const auto& cells : platComps) {
                const int A = (int)cells.size();
                const uint32_t cs = hash2(cells[0] % MW, cells[0] / MW, S + 8100u);  // per plateau, from its first tile
                struct C { int x, y; uint32_t h; };
                auto pick = [&](int dmin, int dmax, int want, int spacing, uint32_t salt) {
                    std::vector<C> cand, got;
                    for (int c : cells) {
                        const int d = platDep[c];
                        if (d >= dmin && d <= dmax && freeAt(c % MW, c / MW) && K(c % MW, c / MW) == 2) cand.push_back({c % MW, c / MW, hash2(c % MW, c / MW, S + salt)});
                    }
                    std::sort(cand.begin(), cand.end(), [](const C& a, const C& b) { return a.h < b.h; });
                    for (const C& c : cand) {
                        bool far = true;
                        for (const C& g : got) far = far && std::max(abs(g.x - c.x), abs(g.y - c.y)) >= spacing;
                        if (far) got.push_back(c);
                        if ((int)got.size() >= want) break;
                    }
                    return got;
                };
                std::vector<std::pair<int, int>> cairnsHere;  // so a tree never stands in front of a cairn
                // the lookout comes first, on the deepest ground of the biggest plateaus: a cairn at the centre-back, and a camp in front of it
                if (A >= 240) {
                    C best{-1, -1, 0};
                    int bd = 0;
                    for (int c : cells) {
                        const int x = c % MW, y = c / MW, d = platDep[c];
                        if (d < 4) continue;
                        const uint32_t h = hash2(x, y, S + 8110u);
                        if (d > bd || (d == bd && h > best.h)) bd = d, best = {x, y, h};
                    }
                    if (best.x >= 0) {
                        const int x = best.x, y = best.y;
                        bool roomy = true;  // a 7 x 5 patch of free top
                        for (int dy = -2; dy <= 2 && roomy; dy++)
                            for (int dx = -3; dx <= 3; dx++)
                                if (!freeAt(x + dx, y + dy) || plat[y + dy][x + dx] != 1) { roomy = false; break; }
                        if (roomy) {
                            setP(P_CAIRN, x, y - 2, 15.f, 0.f);
                            cairnsHere.push_back({x, y - 2});
                            const bool lit = (cs >> 3 & 3u) != 0;  // three in four are lit
                            if (lit) {
                                props.push_back({P_SNOWCAMPFIRE, x * TS + 8.f, y * TS + 13.f, (float)(cs & 7u)});
                                solid[y][x] = true; K(x, y) = 3;
                                lanterns.push_back({x * TS + 8.f, y * TS + 5.f, 1.3f});
                                setP(P_SNOWLOG, x + 2, y, 14.f, 0.f);
                                setP(P_SNOWLOG, x - 1, y + 2, 14.f, 0.f);
                            } else {
                                props.push_back({P_SNOWASH, x * TS + 8.f, y * TS + 13.f, 0.f});
                                solid[y][x] = true; K(x, y) = 3;
                                setP(P_SNOWLOG, x + 2, y + 1, 14.f, 0.f);
                            }
                        }
                    }
                } else if (A >= 130) {  // a smaller plateau gets a cairn alone, if there is deep enough ground
                    auto c = pick(3, 6, 1, 4, 8120u);
                    if (!c.empty() && (cs & 3u) != 0) setP(P_CAIRN, c[0].x, c[0].y, 15.f, 0.f), cairnsHere.push_back({c[0].x, c[0].y});
                }
                if (A >= 90) {  // stones on the middle ground, in twos and threes
                    const int groups = std::min(4, A / 90);
                    for (const C& c : pick(2, 4, groups, 7, 8130u)) {
                        setP(P_SNOWROCK, c.x, c.y, 14.f, 0.f);
                        if (platDep[(size_t)c.y * MW + c.x] >= 3) {
                            const int nb[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                            const uint32_t h = hash2(c.x, c.y, S + 8131u);
                            for (int k = 0; k < 2; k++) {
                                const int* o = nb[(h >> (k * 3) & 3u)];
                                if ((h >> (8 + k) & 1u) == 0) continue;
                                const int nx = c.x + o[0], ny = c.y + o[1];
                                if (freeAt(nx, ny) && plat[ny][nx] == 1 && platDep[(size_t)ny * MW + nx] >= 3) setP(P_SNOWROCK, nx, ny, 14.f, 0.f);
                            }
                        }
                    }
                }
                if (A >= 140) {  // a lone tree on broad ground, never more than three, a few tiles from anything
                    const int trees = std::min(3, A / 140);
                    for (const C& c : pick(3, 6, trees, 8, 8140u)) {
                        bool nearCairn = false;
                        for (auto& cr : cairnsHere) nearCairn = nearCairn || std::max(abs(cr.first - c.x), abs(cr.second - c.y)) < 4;
                        if (nearCairn) continue;
                        setP((hash2(c.x, c.y, S + 8141u) & 255u) < 180u ? P_SNOWPINE : P_BARETREE, c.x, c.y, 15.f, (hash2(c.x, c.y, S + 8142u) & 0xFFFFu) / 65535.f * 6.28f);
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------ 5d. ice spikes (snowfield)
    // Clusters of ice spikes grow where the ground is coldest: along the shores of the frozen lakes and ponds, in the rocky outcrops and, thickest and
    // tallest, in a wide ring round the Frozen Fortress. The map is cut into 12 x 12 tile cells; a cell may hold one cluster of three to five spikes
    // round a centre (the biggest spike at the middle, the others a size or two smaller). Every choice is the tile hash, so no random number is drawn
    // and nothing else on the map moves. A spike blocks its own tile; trails, plazas, landmark yards, plateaus and anything already standing are left alone,
    // and the reachability pass (worldfix) may still remove one that closes a way.
    if (map == B_SNOW && sv2) {
        auto free1 = [&](int x, int y) {
            return inMap(x, y) && !inRing(x, y) && x > 5 && y > 5 && x < MW - 6 && y < MH - 6 && plat[y][x] == 0 && K(x, y) == 0 && !solid[y][x] && (tile[y][x] == T_SNOW || tile[y][x] == T_SNOWP);
        };
        auto nearTrail = [&](int x, int y) {
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    if (inMap(x + dx, y + dy) && K(x + dx, y + dy) == 1) return true;
            return false;
        };
        for (int cyc = 0; cyc < MH / 12; cyc++)
            for (int cxc = 0; cxc < MW / 12; cxc++) {
                const uint32_t h = hash2(cxc, cyc, S + 9101u);
                const int cx = cxc * 12 + 2 + (int)(h % 8u), cy = cyc * 12 + 2 + (int)(h / 8u % 8u);
                if (!inMap(cx, cy)) continue;
                bool shore = false;  // frozen water within four tiles
                for (int dy = -4; dy <= 4 && !shore; dy++)
                    for (int dx = -4; dx <= 4 && !shore; dx++) shore = inMap(cx + dx, cy + dy) && tile[cy + dy][cx + dx] == T_ICE;
                const float fd = hypotf((float)(cx - fortTx), (float)(cy - fortTy));
                const bool fort = fd > 15.f && fd < 36.f;
                const float rock = snowZone(cx, cy).rock;
                float chance = 0.05f;  // a lone cluster in the open
                int base = 0;          // the size of the biggest spike (0-4) is base + a roll
                if (fort) chance = 0.55f, base = 2;
                else if (shore) chance = 0.5f, base = 1;
                else if (rock > 0.55f) chance = 0.28f, base = 1;
                if ((h >> 8 & 1023u) / 1023.f >= chance) continue;
                const int big = std::min(4, base + (int)(h >> 18 & 3u) % (fort ? 3 : 3));
                const int n = 3 + (int)(h >> 22 & 3u) % 3;
                for (int k = 0; k < n; k++) {
                    const uint32_t g = hash2(cx * 7 + k, cy * 5 - k, S + 9102u);
                    const int x = cx + (k == 0 ? 0 : (int)(g % 7u) - 3), y = cy + (k == 0 ? 0 : (int)(g / 7u % 5u) - 2);
                    if (!free1(x, y) || nearTrail(x, y)) continue;
                    const int stage = k == 0 ? big : std::max(0, big - 1 - (int)(g >> 12 & 1u) - (k > 2 ? 1 : 0));
                    const int look = (int)(g >> 16 & 1u);
                    props.push_back({P_ICESPIKE, x * TS + 8.f, y * TS + 14.f, (float)(stage + ICE_SPIKE_STAGES * look)});
                    solid[y][x] = true;
                }
            }
    }

    // ------------------------------------------------------------ 5d. abandoned camps (desert, swamp, mountain): a cold fire and a fallen tent on
    // open ground, four to a map (the snowfield has its own campsites above). One at most in each of six regions, two of the six left out (dealt out
    // by the seed), on the clearest ground that is far from the hub and the landmarks, preferring a spot with a trail within a few tiles. The fire is
    // lit by walking up to it (game_logic.cpp); until then it is only a ring of cold logs.
    if (map == B_DESERT || map == B_SWAMP || map == B_MOUNTAIN) {
        std::vector<char> held((size_t)MW * MH, 0);  // tiles held only by a tree trunk: a camp may clear those away, it makes a clearing
        for (const Prop& p : props)
            if (isTreeKind(p.kind)) held[(size_t)((int)(p.y / TS)) * MW + (int)(p.x / TS)] = 1;
        auto groundOk = [&](int t) {
            switch (map) {
                case B_DESERT: return t == T_SAND;
                case B_SWAMP: return t == T_MUD || t == T_MURK || t == T_DIRT;
                default: return t == T_GRASS || t == T_DARK || t == T_DIRT || t == T_STONE;
            }
        };
        auto openAt = [&](int x, int y) {
            if (!inMap(x, y) || inRing(x, y) || !groundOk(tile[y][x])) return false;
            return solid[y][x] ? (held[(size_t)y * MW + x] && K(x, y) != 1 && K(x, y) != 2) : K(x, y) == 0;
        };
        auto nearLandmark = [&](int x, int y) {
            if (abs(MD.hubTx - x) < 26 && abs(MD.hubTy - y) < 22) return true;
            for (const Landmark& l : landmarks)
                if (abs((int)(l.x / TS) - x) < 26 && abs((int)(l.y / TS) - y) < 22) return true;
            return false;
        };
        std::vector<std::pair<int, int>> sites;
        for (int ry = 0; ry < 2; ry++)
            for (int rx = 0; rx < 3; rx++) {
                if ((rx + 3 * ry + (int)(S % 3u)) % 3 == 0) continue;
                float best = -1.f; int bx = -1, by = -1;
                for (int t = 0; t < 200; t++) {
                    const int x = rx * 80 + 14 + (int)(hash2(rx * 97 + t, ry, S + 7101u) % 52u), y = ry * 84 + 14 + (int)(hash2(rx * 97 + t, ry, S + 7102u) % 56u);
                    bool ok = !nearLandmark(x, y);
                    int trees = 0;
                    for (int dy = -3; dy <= 2 && ok; dy++)
                        for (int dx = -4; dx <= 3 && ok; dx++) { ok = openAt(x + dx, y + dy); trees += ok && solid[y + dy][x + dx]; }
                    if (!ok || trees > 6) continue;
                    float sc = (hash2(x, y, S + 7103u) & 255u) / 1024.f;
                    for (int dy = -8; dy <= 8; dy += 2)
                        for (int dx = -8; dx <= 8; dx += 2)
                            if (inMap(x + dx, y + dy) && K(x + dx, y + dy) == 1) { sc += 0.35f; dy = 9; break; }
                    if (sc > best) best = sc, bx = x, by = y;
                }
                if (bx >= 0) sites.push_back({bx, by});
            }
        for (const auto& c : sites) {
            const int x = c.first, y = c.second;
            // the clearing: every tree trunk standing in the camp's box goes
            props.erase(std::remove_if(props.begin(), props.end(), [&](const Prop& p) {
                if (!isTreeKind(p.kind)) return false;
                const int ptx = (int)(p.x / TS), pty = (int)(p.y / TS);
                if (ptx < x - 4 || ptx > x + 3 || pty < y - 3 || pty > y + 4) return false;
                if (solid[pty][ptx]) { solid[pty][ptx] = false; if (K(ptx, pty) == 3) K(ptx, pty) = 0; }
                return true;
            }), props.end());
            auto block = [&](int tx, int ty) { solid[ty][tx] = true; K(tx, ty) = 3; };
            props.push_back({P_CAMPFIRE, x * TS + 8.f, y * TS + 13.f, (float)camps.size()});
            camps.push_back({x * TS + 8.f, y * TS + 13.f, false});
            block(x, y);
            // astride two tiles, a little way off to the north-west: the tent still standing (ph 1) or come down (ph 0), dealt out so neighbours differ
            props.push_back({P_CAMPTENT, (float)((x - 2) * TS), (y - 1) * TS + 15.f, (float)(((int)camps.size() - 1 + (int)(S & 1u)) & 1)});
            block(x - 3, y - 1); block(x - 2, y - 1);
            decor.erase(std::remove_if(decor.begin(), decor.end(), [&](const Decor& d) { return fabsf(d.x - (x * TS + 8.f)) < 5.f * TS && fabsf(d.y - (y * TS + 8.f)) < 3.5f * TS; }), decor.end());
        }
    }

    // ------------------------------------------------------------ 6. the map's edge wall 
    for (int ty = 0; ty < MH; ty++)
            for (int tx = 0; tx < MW; tx++) {
                if (!inRing(tx, ty) || K(tx, ty) == 1) continue;  // the gate gap stays open
                solid[ty][tx] = true;
                if (!r.chance(0.8f)) continue;
                float px = tx * TS + 8.f + r.range(-2, 2), py = ty * TS + 15.f, ph = r.f() * 6.28f;
                switch (map) {
                    case B_DESERT: props.push_back({r.chance(0.5f) ? P_SANDROCK0 : P_SANDROCK1, px, py, ph}); break;
                    case B_SWAMP: props.push_back({r.chance(0.5f) ? P_DEADTREE0 : P_DEADTREE1, px, py, ph}); break;
                    case B_SNOW: props.push_back({P_SNOWPINE, px, py, ph}); break;
                    case B_HOME: props.push_back({r.chance(0.6f) ? P_PINE : P_OAK0, px, py, ph}); break;
                    default: props.push_back({r.chance(0.6f) ? P_BOULDER : P_PINE, px, py, ph}); break;
                }
            }

    // ------------------------------------------------------------ 7. resource nodes
    auto nearCliff = [&](int tx, int ty, int rad) {
        for (int y = ty - rad; y <= ty + rad; y++)
            for (int x = tx - rad; x <= tx + rad; x++)
                if (inMap(x, y) && tile[y][x] == T_CLIFF) return true;
        return false;
    };
    auto passable = [&](int tx, int ty) { return inMap(tx, ty) && !solid[ty][tx] && !tileSolid(tile[ty][tx]); };
    auto nodeFree = [&](int tx, int ty) {
        if (inRing(tx, ty) || K(tx, ty) != 0 || tile[ty][tx] == T_SHALLOW) return false;
        return passable(tx, ty) && passable(tx + 1, ty) && passable(tx - 1, ty) && passable(tx, ty + 1) && passable(tx, ty - 1);
    };
    auto farFromNodes = [&](float x, float y, float minD) {
        for (auto& n : nodes)
            if (hypotf(n.x - x, n.y - y) < minD) return false;
        return true;
    };
    // a solid vein needs open ground on every side, or a pocket of cliff can wall a player out of it
    auto openAround = [&](int tx, int ty) {
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int x = tx + dx, y = ty + dy;
                if (x < 0 || y < 0 || x >= MW || y >= MH || tile[y][x] == T_CLIFF || solid[y][x]) return false;
            }
        return true;
    };
    auto scatter = [&](int kind, int patches, const std::function<bool(int, int)>& ok, float spacing = 6.f) {
        for (int p = 0; p < patches; p++)
            for (int att = 0; att < 4000; att++) {
                int tx = r.irange(3, MW - 4), ty = r.irange(3, MH - 4);
                if (!nodeFree(tx, ty) || !ok(tx, ty)) continue;
                float px = tx * TS + 8.f, py = ty * TS + 12.f;
                if (!farFromNodes(px, py, spacing * TS)) continue;
                int n = r.irange(1, 3);
                for (int i = 0; i < n; i++) {
                    int ntx = tx + (i == 0 ? 0 : r.irange(-1, 1)), nty = ty + (i == 0 ? 0 : r.irange(-1, 1));
                    if (!nodeFree(ntx, nty) || !ok(ntx, nty)) continue;
                    float nx = ntx * TS + 8.f + r.range(-3, 3), ny = nty * TS + 12.f + r.range(-1, 2);
                    if (!farFromNodes(nx, ny, 10.f)) continue;
                    nodes.push_back({kind, nx, ny, r.f() * 6.28f});
                    if (kind == RN_CACTUS || kind == RN_ORE || kind == RN_COPPER || kind == RN_GOLD) solid[nty][ntx] = true;  // rocks and cacti block the way
                }
                break;
            }
    };
    // patch counts are the original biome's, scaled to the size of one map; rarer things are sparser and need better tools
    if (map == B_DESERT) {
        scatter(RN_CACTUS, 32, [&](int x, int y) { return tile[y][x] == T_SAND || tile[y][x] == T_DUNE; });
        scatter(RN_DESERT_HERB, 28, [&](int x, int y) { return tile[y][x] == T_SAND || tile[y][x] == T_STONE; });
        scatter(RN_DRYWOOD, 28, [&](int x, int y) { return tile[y][x] == T_SAND || tile[y][x] == T_DUNE; }, 5.f);
        scatter(RN_COPPER, 22, [&](int x, int y) { return tile[y][x] == T_SAND || tile[y][x] == T_STONE; });
    } else if (map == B_SWAMP) {
        scatter(RN_REED, 32, [&](int x, int y) { return tile[y][x] == T_MURK || tile[y][x] == T_MUD; });
        scatter(RN_BOGWORT, 28, [&](int x, int y) { return tile[y][x] == T_MUD || tile[y][x] == T_DARK; });
        scatter(RN_NIGHTSHADE, 22, [&](int x, int y) { return tile[y][x] == T_MUD || tile[y][x] == T_DARK; }, 5.f);
        scatter(RN_AMBER, 12, [&](int x, int y) { return tile[y][x] == T_MUD; }, 12.f);
    } else if (map == B_SNOW) {
        scatter(RN_FROSTBLOOM, 44, [&](int x, int y) { return tile[y][x] == T_SNOW || tile[y][x] == T_SNOWP; });
        scatter(RN_ORE, 14, [&](int x, int y) { return tile[y][x] == T_SNOW || tile[y][x] == T_SNOWP; }, 10.f);
        // frostbloom up on the plateaus: the broader the top, the more (one from 90 tiles, two from 240), on the middle ground; the tile hash only
        for (const auto& cells : platComps) {
            const int want = (int)cells.size() >= 240 ? 2 : (int)cells.size() >= 90 ? 1 : 0;
            struct C { int x, y; uint32_t h; };
            std::vector<C> cand;
            for (int c : cells) {
                const int x = c % MW, y = c / MW;
                if (platDep[c] >= 2 && platDep[c] <= 4 && !solid[y][x] && K(x, y) == 2 && !tileSolid(tile[y][x])) cand.push_back({x, y, hash2(x, y, S + 8150u)});
            }
            std::sort(cand.begin(), cand.end(), [](const C& a, const C& b) { return a.h < b.h; });
            int made = 0;
            for (const C& c : cand) {
                if (made >= want) break;
                const float nx = c.x * TS + 8.f, ny = c.y * TS + 12.f;
                if (!farFromNodes(nx, ny, 6.f * TS)) continue;
                nodes.push_back({RN_FROSTBLOOM, nx, ny, (c.h & 0xFFFFu) / 65535.f * 6.28f});
                made++;
            }
        }
    } else if (map == B_MOUNTAIN) {
        scatter(RN_ALPINE_HERB, 28, [&](int x, int y) { return tile[y][x] != T_CLIFF; });
        scatter(RN_ORE, 36, [&](int x, int y) { return tile[y][x] == T_STONE && nearCliff(x, y, 2); });
        scatter(RN_COPPER, 24, [&](int x, int y) { return tile[y][x] == T_STONE; }, 5.f);
        scatter(RN_GOLD, 12, [&](int x, int y) { return tile[y][x] == T_STONE && nearCliff(x, y, 4) && openAround(x, y); }, 7.f);
    }

    // ---- the home forest: herbs, mushrooms, fallen branches and flax. Placed with their own random stream after everything else,
    // so the original home map (terrain, trees, rocks, decor) is exactly what it always was.
    if (home) {
        Rng hr(seed * 104729u + 31u);
        // ---- the glades' features (the clearings themselves are carved in the terrain and tree passes above)
        {
            Rng gf(seed * 7919u + 88u);
            auto gFree = [&](int x, int y) {
                return inMap(x, y) && !inRing(x, y) && (K(x, y) == 0 || K(x, y) == 2) && !solid[y][x] && !tileSolid(tile[y][x]) && tile[y][x] != T_DIRT && tile[y][x] != T_SHALLOW && gladeQ(x, y) < 0.85f;
            };
            // the free tile nearest a glade's middle (a trail may cross it)
            auto gSpot = [&](const Glade& g, int& ox, int& oy) {
                const int cx = (int)lroundf(g.cx), cy = (int)lroundf(g.cy);
                for (int rad = 0; rad <= 3; rad++)
                    for (int dy = -rad; dy <= rad; dy++)
                        for (int dx = -rad; dx <= rad; dx++)
                            if ((abs(dx) == rad || abs(dy) == rad) && gFree(cx + dx, cy + dy)) { ox = cx + dx; oy = cy + dy; return true; }
                return false;
            };
            // a node near (cx, cy) tiles, within `rad` tiles, on a free tile
            auto gNode = [&](int kind, float cx, float cy, float rad) {
                for (int att = 0; att < 80; att++) {  // (the search widens a little each try: a brook may cover the middle of the clearing)
                    const float reach = rad + att * 0.06f;
                    const int tx = (int)lroundf(cx + gf.range(-reach, reach)), ty = (int)lroundf(cy + gf.range(-reach, reach));
                    if (!gFree(tx, ty) || !passable(tx, ty) || !passable(tx + 1, ty) || !passable(tx - 1, ty) || !passable(tx, ty + 1) || !passable(tx, ty - 1)) continue;  // (a trail's cleared margin is fine)
                    const float nx = tx * TS + 8.f + gf.range(-3, 3), ny = ty * TS + 12.f + gf.range(-1, 2);
                    if (!farFromNodes(nx, ny, 14.f)) continue;
                    nodes.push_back({kind, nx, ny, gf.f() * 6.28f});
                    return true;
                }
                return false;
            };
            // decor on a ring `rad` tiles round the middle, skipping tiles that are not open meadow
            auto gRing = [&](const Glade& g, int kind, int n, float rad) {
                for (int i = 0; i < n; i++) {
                    const float a = i * 6.2832f / n + gf.range(-0.2f, 0.2f);
                    const float px = (g.cx + cosf(a) * rad) * TS + 8.f, py = (g.cy + sinf(a) * rad * 0.8f) * TS + 10.f;
                    const int tx = (int)(px / TS), ty = (int)(py / TS);
                    if (inMap(tx, ty) && K(tx, ty) == 0 && !solid[ty][tx] && tile[ty][tx] != T_DIRT && tile[ty][tx] != T_SHALLOW && !tileSolid(tile[ty][tx])) addDecor(kind, px, py);
                }
            };
            for (const Glade& g : glades) {
                int ox, oy;
                switch (g.kind) {
                    case GL_OAK:  // one great old oak in the middle of the clearing
                        if (gSpot(g, ox, oy)) tileProp(P_OAK1, ox, oy);
                        break;
                    case GL_STONE:  // a rune stone (it glows at night) with a scatter of pebbles
                        if (gSpot(g, ox, oy)) addP(P_STANDING, ox, oy);
                        gRing(g, D_PEBBLES, 7, 1.7f);
                        break;
                    case GL_FUNGI:  // a fairy ring of mushrooms, with mushrooms to gather inside it
                        gRing(g, D_MUSHROOM, 11, 2.6f);
                        for (int i = 0; i < 3; i++) gNode(RN_MUSHROOM, g.cx, g.cy, 1.4f);
                        break;
                    case GL_HERBS:  // a herb patch
                        for (int i = 0; i < 4; i++) gNode(RN_HERB, g.cx, g.cy, 2.6f);
                        for (int i = 0; i < 2; i++) gNode(RN_FLAX, g.cx, g.cy, 3.f);
                        break;
                    case GL_CAMP: {  // a hunter's rest: a ring of stones where a fire once burned, a log on each side
                        gRing(g, D_PEBBLES, 9, 1.2f);
                        for (int side = -1; side <= 1; side += 2) {
                            const int tx = (int)lroundf(g.cx) + side * 3, ty = (int)lroundf(g.cy) + 1;
                            if (gFree(tx, ty)) {
                                props.push_back({P_LOG, tx * TS + 8.f, ty * TS + 14.f, 0});
                                solid[ty][tx] = true;
                            }
                        }
                        break;
                    }
                    default: break;  // GL_FLOWERS: the flowers themselves (the tree pass) are the feature
                }
            }
            // a flower field has herbs to gather among the flowers
            for (const FlowerField& f : fields)
                for (int i = 0, placed = 0; i < 90 && placed < 7; i++) {
                    const int tx = (int)lroundf(f.cx + gf.range(-f.rx, f.rx) * 0.8f), ty = (int)lroundf(f.cy + gf.range(-f.ry, f.ry) * 0.8f);
                    if (!inMap(tx, ty) || fieldQ(tx, ty) > 0.7f || inRing(tx, ty) || K(tx, ty) != 0 || solid[ty][tx] || tile[ty][tx] != T_GRASS) continue;
                    if (!passable(tx, ty) || !passable(tx + 1, ty) || !passable(tx - 1, ty) || !passable(tx, ty + 1) || !passable(tx, ty - 1)) continue;
                    const float nx = tx * TS + 8.f + gf.range(-3, 3), ny = ty * TS + 12.f + gf.range(-1, 2);
                    if (!farFromNodes(nx, ny, 16.f)) continue;
                    nodes.push_back({RN_HERB, nx, ny, gf.f() * 6.28f});
                    placed++;
                }
        }
        auto homeOk = [&](int tx, int ty) {
            const int cx = tx - COX, cy = ty - COY;
            if (cx < 4 || cy < 4 || cx >= CW - 4 || cy >= CH - 4 || K(tx, ty) != 0) return false;
            if (hypotf(cx - 44.f, (cy - 30.f) * 1.15f) < 11.f) return false;  // the village plaza stays clear
            return passable(tx, ty) && passable(tx + 1, ty) && passable(tx - 1, ty) && passable(tx, ty + 1) && passable(tx, ty - 1) && tile[ty][tx] != T_DIRT && tile[ty][tx] != T_SHALLOW;
        };
        auto homeScatter = [&](int kind, int count, const std::function<bool(int, int)>& ok) {
            int placed = 0;
            for (int att = 0; att < count * 60 && placed < count; att++) {
                int tx = hr.irange(COX + 4, COX + CW - 5), ty = hr.irange(COY + 4, COY + CH - 5);
                if (!homeOk(tx, ty) || !ok(tx, ty)) continue;
                float nx = tx * TS + 8.f + hr.range(-3, 3), ny = ty * TS + 12.f + hr.range(-1, 2);
                if (!farFromNodes(nx, ny, 3.f * TS)) continue;
                nodes.push_back({kind, nx, ny, hr.f() * 6.28f});
                placed++;
            }
        };
        homeScatter(RN_HERB, 34, [&](int x, int y) { return tile[y][x] == T_GRASS || tile[y][x] == T_DARK; });
        homeScatter(RN_MUSHROOM, 26, [&](int x, int y) { return tile[y][x] == T_DARK; });
        homeScatter(RN_BRANCH, 36, [&](int x, int y) { return (tile[y][x] == T_GRASS || tile[y][x] == T_DARK) && nearTree(x * TS + 8.f, y * TS + 12.f, 40.f); });  // branches fall from trees
        homeScatter(RN_FLAX, 28, [&](int x, int y) { return tile[y][x] == T_GRASS; });
        // and the same plants through the home forest around the original area
        auto outer = [&](int x, int y) { return !inCore(x, y); };
        scatter(RN_HERB, 70, [&](int x, int y) { return outer(x, y) && (tile[y][x] == T_GRASS || tile[y][x] == T_DARK || tile[y][x] == T_MOSS || tile[y][x] == T_LEAF); }, 4.f);
        scatter(RN_MUSHROOM, 40, [&](int x, int y) { return outer(x, y) && (tile[y][x] == T_DARK || tile[y][x] == T_MOSS || tile[y][x] == T_ROOTS); }, 4.f);
        scatter(RN_BRANCH, 70, [&](int x, int y) { return outer(x, y) && tileGrassy(tile[y][x]) && nearTree(x * TS + 8.f, y * TS + 12.f, 40.f); }, 4.f);
        scatter(RN_FLAX, 50, [&](int x, int y) { return outer(x, y) && tile[y][x] == T_GRASS; }, 4.f);
        // ---- lake banks: a ring of trodden earth round every lake, one tile wide and ragged (a second tile here and there). Made after every tree,
        // plant and random choice above, so none of them depends on it; only forest floors turn into bank (paths, brooks and glades' meadows stay)
        // ---- lake depth as real tiles: round the shore the water is shallow (wade), one tile further in it is mid (wade, slow), and the rest stays
        // deep (T_WATER, solid). A brook or any dry tile counts as shore. The village area is left alone (the town is laid over it afterwards).
        {
            std::vector<std::pair<int, int>> shallow, mid;
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++) {
                    if (tile[y][x] != T_WATER || inCore(x, y) || inRing(x, y)) continue;
                    int d = 3;
                    for (int dy = -2; dy <= 2; dy++)
                        for (int dx = -2; dx <= 2; dx++)
                            if (inMap(x + dx, y + dy) && tile[y + dy][x + dx] != T_WATER) d = std::min(d, std::max(std::abs(dx), std::abs(dy)));
                    if (d == 1) shallow.push_back({x, y});
                    else if (d == 2) mid.push_back({x, y});
                }
            for (auto& pt : shallow) tile[pt.second][pt.first] = T_LAKE_SHALLOW;
            for (auto& pt : mid) tile[pt.second][pt.first] = T_LAKE_MID;
        }
        {
            auto bankable = [](uint8_t t) { return t == T_GRASS || t == T_DARK || t == T_LEAF || t == T_NEEDLES || t == T_MOSS; };
            std::vector<std::pair<int, int>> todo;
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++) {
                    if (!bankable(tile[y][x]) || inCore(x, y)) continue;
                    int dmin = 3;
                    for (int dy = -2; dy <= 2; dy++)
                        for (int dx = -2; dx <= 2; dx++)
                            if (inMap(x + dx, y + dy) && tileLake(tile[y + dy][x + dx])) dmin = std::min(dmin, std::max(std::abs(dx), std::abs(dy)));
                    if (dmin == 1 || (dmin == 2 && fbm(x * 0.4f, y * 0.4f, S + 9) < 0.47f)) todo.push_back({x, y});
                }
            for (auto& pt : todo) tile[pt.second][pt.first] = T_BANK;
        }
        generateTown();  // last: the town is laid over the middle of the original area
    }
}

// ------------------------------------------------------------------------------------------------------------------------------
// The central town (town.hpp). Laid over the middle of the original Wildwood area after everything else is generated, so the surrounding
// terrain, forest and resources are exactly as before: only the town's own rectangle is cleared (trees, rocks, plants and the old hut go),
// paved with streets and filled with the six buildings and their street furniture. Streets run to the plaza and join the old dirt paths.
inline bool World::inTown(float x, float y, float margin) const {
    return mapId == B_HOME && x >= (COX + TOWN_X0) * TS - margin && x < (COX + TOWN_X1) * TS + margin && y >= (COY + TOWN_Y0) * TS - margin &&
           y < (COY + TOWN_Y1) * TS + margin;
}

inline void World::generateTown() {
    const int X0 = COX + TOWN_X0, Y0 = COY + TOWN_Y0, X1 = COX + TOWN_X1, Y1 = COY + TOWN_Y1;  // map tiles, X1 / Y1 exclusive
    auto inRect = [&](float px, float py) { return px >= X0 * TS && px < X1 * TS && py > Y0 * TS && py <= Y1 * TS; };
    // 0. nothing gatherable in the town, beside it or on its four roads: no tree, rock, plant or ore to chop, mine or pick (the thick
    // forest around the town is ordinary forest)
    {
        const int M = 2;  // tiles of clearing around the rectangle
        auto zone = [&](float px, float py) {
            const int tx = (int)(px / TS) - COX, ty = (int)((py - 1.f) / TS) - COY;
            if (tx >= TOWN_X0 - M && tx < TOWN_X1 + M && ty >= TOWN_Y0 - M && ty < TOWN_Y1 + M) return true;  // the town and its margin
            if (ty >= 23 && ty <= 27 && ((tx >= 12 && tx < TOWN_X0) || (tx >= TOWN_X1 && tx < 76))) return true;        // west / east road
            if (tx >= 42 && tx <= 45 && ((ty >= 8 && ty < TOWN_Y0) || (ty >= TOWN_Y1 && ty < 52))) return true;         // north / south road
            return false;
        };
        auto gatherable = [](int kind) { return kind == P_OAK0 || kind == P_OAK1 || kind == P_PINE || kind == P_ROCK0 || kind == P_ROCK1 || kind == P_LOG; };
        for (size_t i = 0; i < props.size();) {
            if (gatherable(props[i].kind) && zone(props[i].x, props[i].y)) {
                solid[(int)((props[i].y - 1.f) / TS)][(int)(props[i].x / TS)] = false;
                props.erase(props.begin() + i);
            } else
                i++;
        }
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const ResNode& n) { return zone(n.x, n.y); }), nodes.end());
    }
    // 1. clear the rectangle: props (the campfire stays), decor, resource nodes, solid marks, water
    props.erase(std::remove_if(props.begin(), props.end(), [&](const Prop& p) { return p.kind != P_FIRE && inRect(p.x, p.y); }), props.end());
    decor.erase(std::remove_if(decor.begin(), decor.end(), [&](const Decor& d) { return inRect(d.x, d.y); }), decor.end());
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const ResNode& n) { return inRect(n.x, n.y); }), nodes.end());
    for (int y = Y0; y < Y1; y++)
        for (int x = X0; x < X1; x++) {
            solid[y][x] = false;
            if (tile[y][x] == T_WATER || tile[y][x] == T_DIRT) tile[y][x] = T_GRASS;
        }
    markSolidAt(fire.x, fire.y - 3);
    // 2. streets: two east-west streets in front of the building rows, a north-south road through the middle, a paved plaza
    auto paved = [&](int cx0, int cy0, int cx1, int cy1) {  // core tile rectangle, inclusive
        for (int y = COY + cy0; y <= COY + cy1; y++)
            for (int x = COX + cx0; x <= COX + cx1; x++)
                if (x >= 2 && y >= 2 && x < MW - 2 && y < MH - 2 && tile[y][x] != T_WATER) tile[y][x] = T_COBBLE;
    };
    paved(TOWN_X0, 24, TOWN_X1 - 1, 26);   // street A, in front of the north row
    paved(TOWN_X0, 37, TOWN_X1 - 1, 38);   // street B, in front of the south row
    paved(43, TOWN_Y0, 44, TOWN_Y1 - 1);   // the road through the middle
    paved(38, 27, 50, 33);                 // the plaza around the campfire
    // the four old dirt paths run out of the town: pave their first stretch so they meet the streets
    for (int y = COY + 8; y < COY + TOWN_Y0; y++)
        for (int x = COX + 43; x <= COX + 44; x++) tile[y][x] = T_COBBLE;
    for (int y = COY + TOWN_Y1; y < COY + 52; y++)
        for (int x = COX + 43; x <= COX + 44; x++) tile[y][x] = T_COBBLE;
    for (int x = COX + 12; x < COX + TOWN_X0; x++)
        for (int y = COY + 24; y <= COY + 26; y++) tile[y][x] = T_COBBLE;
    for (int x = COX + TOWN_X1; x < COX + 76; x++)
        for (int y = COY + 24; y <= COY + 26; y++) tile[y][x] = T_COBBLE;
    // 3. the buildings
    for (int k = 0; k < BLD_COUNT; k++) {
        props.push_back({P_BLD0 + k, bldCenterX(k, COX), bldBaseY(k, COY), 0});
        for (int y = 0; y < BLD_H; y++)
            for (int x = 0; x < BLD_W; x++) solid[COY + BUILDINGS[k].cy + y][COX + BUILDINGS[k].cx + x] = true;
        // a lamp by every door, lit at night
        const float lx = bldCenterX(k, COX) + 34.f, ly = bldBaseY(k, COY) + 12.f;
        props.push_back({P_LAMP, lx, ly, 0});
        lanterns.push_back({lx, ly - 24.f, 0.8f});
    }
    // 4. street furniture: lamps along both streets, fences behind the north row and along the south edge (gaps for the roads), benches, planters, the well, trees
    auto at = [&](int cx, float cyf) { return V2((COX + cx) * (float)TS + 8.f, (COY + cyf) * (float)TS); };
    for (int cx : {28, 36, 41, 46, 52, 60}) {
        const V2 a = at(cx, 27.f);
        props.push_back({P_LAMP, a.x, a.y - 2.f, 0});
        lanterns.push_back({a.x, a.y - 26.f, 0.75f});
    }
    for (int cx : {28, 40, 47, 56}) {
        const V2 a = at(cx, 39.f);
        props.push_back({P_LAMP, a.x, a.y - 2.f, 0});
        lanterns.push_back({a.x, a.y - 26.f, 0.75f});
    }
    for (int cx = TOWN_X0; cx < TOWN_X1; cx++) {
        if (cx >= 42 && cx <= 45) continue;  // the road
        for (int cy : {18, 42}) {
            const V2 a = at(cx, cy + 0.9f);
            props.push_back({P_FENCE, a.x, a.y, 0});
            solid[COY + cy][COX + cx] = true;
        }
    }
    const int bench[][2] = {{40, 28}, {47, 33}, {40, 33}, {50, 28}};
    for (auto& b : bench) {
        const V2 a = at(b[0], b[1] + 0.9f);
        props.push_back({P_BENCH, a.x + 4.f, a.y, 0});
        solid[COY + b[1]][COX + b[0]] = true;
    }
    const int planters[][2] = {{29, 37}, {36, 37}, {51, 37}, {58, 37}, {27, 27}, {62, 27}};
    for (auto& b : planters) {
        const V2 a = at(b[0], b[1] + 0.95f);
        props.push_back({P_PLANTER, a.x, a.y, 0});
        solid[COY + b[1]][COX + b[0]] = true;
    }
    {
        const V2 a = at(38, 31.9f);
        props.push_back({P_WELL, a.x + 8.f, a.y, 0});
        solid[COY + 31][COX + 38] = solid[COY + 31][COX + 39] = true;
    }
    {   // the Homestead Portal: a standing-stone gate on the west side of the plaza, between the flower shop and the well
        const V2 a = at(33, 31.9f);
        props.push_back({P_HPORTAL, a.x, a.y, 0});
        for (int i = 0; i < 3; i++) solid[COY + 31][COX + 32 + i] = true;
        hportal = a;
        lanterns.push_back({a.x, a.y - 22.f, 0.9f});
    }
    {   // town signs where the streets leave the town
        const V2 w = at(TOWN_X0 - 1, 28.6f), e = at(TOWN_X1, 28.6f);
        props.push_back({P_SIGN, w.x, w.y, 0});
        props.push_back({P_SIGN, e.x, e.y, 0});
        markSolidAt(w.x, w.y - 2.f);
        markSolidAt(e.x, e.y - 2.f);
    }
    Rng tr(seed * 31337u + 4);
    const int trees[][2] = {{26, 31}, {61, 31}, {25, 40}, {62, 40}, {35, 18}, {52, 18}, {27, 17}, {60, 17}};
    for (auto& t : trees) {
        const V2 a = at(t[0], t[1] + 0.9f);
        props.push_back({tr.chance(0.5f) ? P_OAK0 : P_PINE, a.x, a.y, tr.f() * 6.28f});
        solid[COY + t[1]][COX + t[0]] = true;
    }
    // flowers along the plaza edge (decor)
    for (int i = 0; i < 40; i++) {
        const int cx = 24 + (int)(tr.f() * 40), cy = 27 + (int)(tr.f() * 7);
        if (tile[COY + cy][COX + cx] == T_COBBLE || solid[COY + cy][COX + cx]) continue;
        decor.push_back({tr.irange(0, 2), (COX + cx) * (float)TS + tr.range(2, 14), (COY + cy) * (float)TS + tr.range(6, 15), tr.f() * 6.28f});
    }
    // 5. where things are now: the hunter starts on the plaza; the old shop hut is gone (the General Store is a building)
    spawn = V2((COX + 43.5f) * TS, (COY + 30.f) * TS);
    questGiver = V2((COX + 45.75f) * TS, (COY + 31.6f) * TS);
    hut = keeper = V2(-99999.f, -99999.f);
    door = V2(bldDoorX(BLD_SHOP, COX), bldDoorY(BLD_SHOP, COY));
}

#include "farming/homestead_gen.hpp"

// Keeps trees (oaks, pines, snow spruces, dead and bare trees) from piling on top of each other. Runs once after a map is generated: walking the trees in order, a tree is
// dropped when its canopy would cover more than TREE_OVERLAP (a share, 0 to 1) of an already kept tree's canopy (tree_lime.hpp). Rocks, bushes and everything else are left alone. Nothing random is used, so the map stays the same every run.
inline void World::thinTrees() {
    struct T { float x, y, w, c; };  // c = height of the leafy part
    const float CELL = 64.f;
    const int gw = (int)((MW * TS) / CELL) + 2, gh = (int)((MH * TS) / CELL) + 2;
    std::vector<std::vector<T>> grid((size_t)gw * gh);
    std::vector<Prop> out;
    out.reserve(props.size());
    for (const Prop& p : props) {
        const bool tree = p.kind == P_OAK0 || p.kind == P_OAK1 || p.kind == P_PINE || p.kind == P_SNOWPINE || p.kind == P_DEADTREE0 ||
                          p.kind == P_DEADTREE1 || p.kind == P_BARETREE;
        if (!tree) { out.push_back(p); continue; }
        const int tx = (int)(p.x / TS), ty = (int)(p.y / TS);
        float w = PINE_W, h = PINE_H;  // the size of the tree's sprite: how far its top reaches sideways and up
        if (p.kind == P_OAK0 || p.kind == P_OAK1) { const OakVar& v = OAK_VAR[oakVariantFor(tx, ty)]; w = (float)v.w; h = (float)v.h; }
        else if (p.kind == P_SNOWPINE) { w = SNOW_SPRUCE_W; h = SNOW_SPRUCE_H; }
        else if (p.kind != P_PINE) { w = DEAD_SPRUCE_W; h = DEAD_SPRUCE_H; }
        const T t{p.x, p.y, w, h * 0.7f};
        const int cx = (int)(p.x / CELL), cy = (int)(p.y / CELL);
        // the trees of the map's edge wall stay: they are walls, not forest
        const bool wall = tx < 2 || ty < 2 || tx >= MW - 2 || ty >= MH - 2;
        bool crowded = false;
        for (int gy = std::max(0, cy - 1); gy <= std::min(gh - 1, cy + 1) && !crowded; gy++)
            for (int gx = std::max(0, cx - 1); gx <= std::min(gw - 1, cx + 1) && !crowded; gx++)
                for (const T& o : grid[(size_t)gy * gw + gx]) {
                    // how far the two tops cover each other sideways and up/down (0 = not at all, 1 = fully)
                    const float ow = std::max(0.f, std::min(1.f, ((t.w + o.w) * 0.5f - std::fabs(t.x - o.x)) / std::min(t.w, o.w)));
                    const float oh = std::max(0.f, std::min(1.f, ((t.c + o.c) * 0.5f - std::fabs(t.y - o.y)) / std::min(t.c, o.c)));
                    // ow * oh is the share of the smaller top that is covered. A tree straight behind another (same sideways spot)
                    // covers a lot, so it must stand far back. A tree shifted to the side covers less, so it may stand closer:
                    // neighbours end up staggered in rows, like layers.
                    if (!wall && ow * oh > TREE_OVERLAP) { crowded = true; break; }
                }
        if (crowded) {
            // the tree is gone, so is its blocking tile
            if (tx >= 0 && ty >= 0 && tx < MW && ty < MH) solid[ty][tx] = false;
            continue;
        }
        grid[(size_t)cy * gw + cx].push_back(t);
        out.push_back(p);
    }
    props.swap(out);
}

// Gives every rock one of its 5 looks and makes it block as wide as that sprite is. A look is used only when all the extra tiles it
// needs are free, plain ground (not a trail, water, a cliff or something already blocking); otherwise the next look is tried, and a
// rock with no room blocks only its own tile. The map's edge wall keeps one tile per boulder, it is a wall already.
inline void World::fitRocks() {
    auto plain = [&](int tx, int ty) {
        if (tx < 2 || ty < 2 || tx >= MW - 2 || ty >= MH - 2) return false;
        if (solid[ty][tx] || tileSolid(tile[ty][tx])) return false;
        const uint8_t t = tile[ty][tx];
        return t != T_DIRT && t != T_COBBLE && t != T_WATER && t != T_CLIFF && t != T_RAIL && t != T_ICE;
    };
    // The sprite boxes of the rocks kept so far. A rock whose picture would cover more than ROCK_OVERLAP of a kept rock's picture
    // (the share of the smaller one) tries its other looks, and is dropped when none fits: boulders first, then small, then tiny rocks.
    struct Box { float x0, y0, x1, y1; };
    std::vector<Box> kept;
    auto boxOf = [&](const Prop& p, int look) {
        const RockArt& a = rockSet(p.kind)[look];
        return Box{p.x - a.w * 0.5f, p.y - a.h + 2.f, p.x + a.w * 0.5f, p.y + 2.f};
    };
    auto crowds = [&](const Box& b) {
        const float ba = (b.x1 - b.x0) * (b.y1 - b.y0);
        for (const Box& o : kept) {
            const float iw = std::min(b.x1, o.x1) - std::max(b.x0, o.x0), ih = std::min(b.y1, o.y1) - std::max(b.y0, o.y0);
            if (iw <= 0 || ih <= 0) continue;
            const float oa = (o.x1 - o.x0) * (o.y1 - o.y0);
            if (iw * ih > ROCK_OVERLAP * std::min(ba, oa)) return true;
        }
        return false;
    };
    std::vector<size_t> order;
    for (int pass = 0; pass < 3; pass++)
        for (size_t i = 0; i < props.size(); i++) {
            const int k = props[i].kind;
            const int rc = rockClass(k);
            if (rc >= 0 && rc == 2 - pass) order.push_back(i);
        }
    std::vector<bool> drop(props.size(), false);
    for (size_t i : order) {
        Prop& p = props[i];
        const int tx = (int)(p.x / TS), ty = (int)(p.y / TS);
        const int start = std::abs((tx * 7 + ty * 13)) % ROCK_LOOKS;
        if (tx < 2 || ty < 2 || tx >= MW - 2 || ty >= MH - 2) { p.ph = 100.f + start; kept.push_back(boxOf(p, start)); continue; }  // the edge wall stays
        bool done = false;
        for (int k = 0; k < ROCK_LOOKS && !done; k++) {
            const int look = (start + k) % ROCK_LOOKS;
            if (crowds(boxOf(p, look))) continue;
            int x0, y0, x1, y1;
            rockFoot(p, look, false, x0, y0, x1, y1);
            bool ok = true;
            for (int y = y0; y <= y1 && ok; y++)
                for (int x = x0; x <= x1 && ok; x++)
                    if (!(x == tx && y == ty) && !plain(x, y)) ok = false;
            if (!ok) continue;
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++) solid[y][x] = true;
            p.ph = (float)look;
            kept.push_back(boxOf(p, look));
            done = true;
        }
        if (done) continue;
        // no look fits without a crowded picture or a blocked footprint: a boulder stays as it is (blocking its own tile), the rest go
        bool free = true;
        for (int k = 0; k < ROCK_LOOKS && free; k++) free = !crowds(boxOf(p, (start + k) % ROCK_LOOKS));
        if (p.kind == P_BOULDER || p.kind == P_SANDBOULDER || free) {
            p.ph = 100.f + start;
            solid[ty][tx] = true;
            kept.push_back(boxOf(p, start));
        } else {
            drop[i] = true;
            solid[ty][tx] = false;  // it blocked its own tile (placed that way): the tile is free again
        }
    }
    std::vector<Prop> out;
    out.reserve(props.size());
    for (size_t i = 0; i < props.size(); i++)
        if (!drop[i]) out.push_back(props[i]);
    props.swap(out);
}

inline void World::setPropSolid(const Prop& p, bool v) {
    int x0, y0, x1, y1;
    if (isRockKind(p.kind)) rockFoot(p, x0, y0, x1, y1);
    else x0 = x1 = (int)(p.x / TS), y0 = y1 = (int)(p.y / TS);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (x >= 0 && y >= 0 && x < MW && y < MH) solid[y][x] = v;
}

// Marks the tiles that stop light. Buildings, huts, cave mouths and wide rocks stand on solid tiles: around each one the solid tiles
// become occluders (trees do not, a forest would be pitch black). Cliffs and walls count too.
inline void World::buildOccluders() {
    memset(occ, 0, sizeof(occ));
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
            if (tile[y][x] == T_CLIFF) occ[y][x] = true;
    auto mark = [&](const Prop& p, int rx, int up) {
        const int tx = (int)(p.x / TS), ty = (int)(p.y / TS);
        for (int y = ty - up; y <= ty; y++)
            for (int x = tx - rx; x <= tx + rx; x++)
                if (x >= 0 && y >= 0 && x < MW && y < MH && solid[y][x]) occ[y][x] = true;
    };
    for (const Prop& p : props) {
        if (p.kind >= P_BLD0 && p.kind <= P_BLD5) mark(p, 5, 5);
        else if (p.kind == P_HUT || p.kind == P_ICEHUT) mark(p, 2, 2);
        else if (p.kind == P_CAVE) mark(p, 3, 3);
        else if (isRockKind(p.kind)) {
            int x0, y0, x1, y1;
            rockFoot(p, x0, y0, x1, y1);
            if (x1 > x0 || p.kind == P_BOULDER || p.kind == P_SANDBOULDER)
                for (int y = y0; y <= y1; y++)
                    for (int x = x0; x <= x1; x++)
                        if (x >= 0 && y >= 0 && x < MW && y < MH) occ[y][x] = true;
        }
    }
}
