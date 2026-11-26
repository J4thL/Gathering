// game_render_world.cpp - drawing the world: ground, props, animals, the hunter, weather and light.
#include <cstdio>
#include "core/game.hpp"
#include "core/logic_util.hpp"


// ================================================================= ground edge blending
// which ground tiles blend into each other (tile_edges.hpp); everything else (water, ice, cliffs, interiors) keeps its hard edge
static int blendClass(uint8_t t) {
    switch (t) {
        case T_GRASS: return BL_GRASS;
        case T_DARK: return BL_DARK;
        case T_LEAF: return BL_LEAF;
        case T_NEEDLES: return BL_NEEDLES;
        case T_MOSS: return BL_MOSS;
        case T_BANK: return BL_BANK;
        case T_ROOTS: return BL_NEEDLES;  // (never generated any more; an old map would show needles)
        case T_DIRT: return BL_DIRT;
        case T_SAND: return BL_SAND;
        case T_DUNE: return BL_DUNE;
        case T_MUD: return BL_MUD;
        case T_SNOW: return BL_SNOW;
        case T_SNOWP: return BL_SNOWP;
        case T_STONE: return BL_STONE;
        case T_ICE: return BL_ICE;  // never grows over anything (lowest priority, no texture of its own to grow), only receives the snow round a lake
        default: return -1;
    }
}
// draws, over tile (tx, ty), the texture of every neighbouring ground type of a higher priority, grown a few pixels into it
void Game::blendEdges(int tx, int ty, int sx, int sy, int cls, uint32_t h) {
    static const int DX[8] = {0, 1, 0, -1, 1, 1, -1, -1}, DY[8] = {-1, 0, 1, 0, -1, 1, 1, -1};  // N E S W NE SE SW NW
    int nc[8], present = 0;
    for (int d = 0; d < 8; d++) {
        const int nx = tx + DX[d], ny = ty + DY[d];
        nc[d] = (nx >= 0 && ny >= 0 && nx < MW && ny < MH) ? blendClass(world.tile[ny][nx]) : -1;
        if (nc[d] >= 0 && BLEND_PRIO[nc[d]] > BLEND_PRIO[cls]) present |= 1 << nc[d];
    }
    if (!present) return;
    for (int pass = 0; pass < BL_COUNT; pass++) {  // lowest priority first, so the highest ends up on top
        int c = -1;
        for (int k = 0; k < BL_COUNT; k++)
            if ((present >> k & 1) && (c < 0 || BLEND_PRIO[k] < BLEND_PRIO[c])) c = k;
        if (c < 0) break;
        present &= ~(1 << c);
        int cfg = 0;
        for (int d = 0; d < 8; d++)
            if (nc[d] == c) cfg |= 1 << d;
        if (const Spr* s = A.blend.get(ren, c, (int)(h % 7), cfg, (int)((h >> 5) & 3))) sprS(*s, sx, sy);
    }
}

// ================================================================= world
// How much of the snowfield's wind detail (the ripple crests and the streaks) a tile gets, 0..1.2: most on the drifted slopes, a little less on open tundra,
// little in the forest (the trees shelter the ground), half on a plateau's top, and next to nothing within a tile of a rock wall, so the lines never run up
// against the rock.
static float snowFxK(const World& w, int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= MW || ty >= MH) return 0.f;
    float k = 0.75f + 0.45f * (w.snowSlope[ty][tx] / 255.f) - 0.55f * (w.snowForest[ty][tx] / 255.f);
    if (w.plat[ty][tx] == 1) k *= 0.5f;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            const int nx = tx + dx, ny = ty + dy;
            if (nx >= 0 && ny >= 0 && nx < MW && ny < MH && w.tile[ny][nx] == T_CLIFF) return clampf(k, 0.f, 1.2f) * 0.12f;
        }
    return clampf(k, 0.f, 1.2f);
}

// The snowfield's wind on the ground: faint streaks of snow slide east across the open snow. They are fixed to the world (a grid of cells, some of them with a
// streak that runs along its cell and fades in and out), so they move with the wind, not with the camera. How many there are and how bright depends on the wind
// (a calm day has only a few), they taper away at both ends, they are drawn a piece at a time so they stop at a rock wall, a log or a tree and fade where
// snowFxK says the ground is sheltered, and they dim with the daylight and turn pale blue at night.
void Game::drawSnowDrift() {
    const float wd = clampf(wind + 0.8f * galeK, 0.f, 1.f);  // 0 calm .. 1 gale
    const float dens = 0.14f + 0.6f * wd;                     // the share of cells that carry a streak
    const float dl = daylight();
    const float bright = (0.35f + 0.65f * wd) * (0.45f + 0.55f * dl), cool = 1.f - dl;
    const int CW = 64, CH = 34, SEG = 6;
    const u32 wr = (u32)(255 - 55 * cool), wgc = (u32)(255 - 41 * cool), wb = (u32)(255 - 13 * cool);  // white by day, a pale blue by night
    for (int gy = camY / CH - 1; gy <= (camY + LH) / CH + 1; gy++)
        for (int gx = camX / CW - 1; gx <= (camX + LW) / CW + 1; gx++) {
            const uint32_t h = hash2(gx, gy, 9137u);
            if ((float)(h >> 2 & 255u) / 255.f >= dens) continue;
            const int len = 22 + (int)(h >> 4 & 15u) * 3;
            const float span = (float)(CW + len), pos = fmodf(rainDrift * 0.55f + (float)(h >> 8 & 1023u), span);
            const float env = sinf(pos / span * 3.1416f);
            if (env * bright < 0.03f) continue;
            const int x = gx * CW + (int)pos - len, y = gy * CH + (int)(h >> 20 & 31u) % CH;
            if (x < 0 || y < 0) continue;
            for (int i = 0; i < SEG; i++) {
                const int x0 = x + len * i / SEG, x1 = x + len * (i + 1) / SEG, sl = x1 - x0;
                const int tx = (x0 + x1) / 2 / TS, ty = y / TS;
                if (tx >= MW || ty >= MH || (!world.tileIs(tx, ty, T_SNOW) && !world.tileIs(tx, ty, T_SNOWP)) || world.solid[ty][tx]) continue;
                const int a = (int)(160.f * env * sinf((i + 0.5f) / SEG * 3.1416f) * bright * snowFxK(world, tx, ty));  // thin at both ends
                if (a < 6) continue;
                fillRect(x0 - camX, y - camY, sl, 1, RGBA(wr, wgc, wb, a));
                if (sl > 1) fillRect(x0 + 1 - camX, y + 1 - camY, sl - 1, 1, RGBA(138, 164, 202, a * 7 / 10));
            }
        }
}

// ---- snowfield ground detail
// Two FastNoiseLite layers over the snow tiles, both stretched along the wind (east): broad drifts, a pale crest of snow where the wind has piled it up
// and a bluish shade in the hollows (dithered, so it stays pixel art), and thin ripples along the drifts, each a lit pixel with its shadow just below.
// The noise is sampled at world pixels, so a drift carries on over the tile borders instead of repeating per tile like the tile art does.
void Game::freeSnowDetail() {
    for (auto& kv : snowDetTex)
        if (kv.second) SDL_DestroyTexture(kv.second);
    snowDetTex.clear();
    for (auto& kv : snowFaceCache)
        if (kv.second.t) SDL_DestroyTexture(kv.second.t);
    snowFaceCache.clear();
}

// The rock of a snow cliff: columns of frosty basalt standing side by side, each with a lit left edge, a shaded right edge and a dark joint here and there
// where one block ends and the next begins; the columns are 3-5 px wide and laid out in world pixels (so they run on over the tile borders and do not repeat
// tile by tile), and the rock darkens towards the foot. With `cap` the top of the tile carries the snow, its lower edge scalloped.
static Img snowFaceBase(FastNoiseLite& cell, int tx, int ty, bool cap) {
    (void)cell;
    Img im(TS, TS);
    // the snowfield's rock palette (ROCK_PALS[B_SNOW]: pale frosty blue), the same stone as the icy rocks standing on the plateaus
    static const u32 TONE[3] = {HEX(0x7a86a6), HEX(0x727e9e), HEX(0x838fad)};
    static const u32 LOW[3] = {HEX(0x66739a), HEX(0x606c92), HEX(0x6d7aa0)};  // the same, darker, towards the foot
    const u32 LT = HEX(0xb2bed6), SH = HEX(0x4a5575), DK = HEX(0x3c4660);
    auto edgeOf = [](int k) { return 4 * k + (int)(hash2(k, 11, 6221u) % 3u) - 1; };  // where column k begins: 4 px apart, jittered by one
    for (int y = 0; y < TS; y++)
        for (int x = 0; x < TS; x++) {
            const int X = tx * TS + x, Y = ty * TS + y;
            int k = (X >> 2) + 1;  // the column that holds X: the last one that begins at or before it
            while (edgeOf(k) > X) k--;
            const int xin = X - edgeOf(k), w = edgeOf(k + 1) - edgeOf(k);
            const uint32_t hk = hash2(k, 5, 6223u);
            const int seg = 6 + (int)(hk % 5u), off = (int)((hk >> 8) % 11u);  // blocks 6-10 px tall, offset per column
            const bool joint = (Y + off) % seg == 0;
            const bool lowRows = y >= 10 && !cap;
            u32 c = (lowRows ? LOW : TONE)[(hk >> 4) % 3u];
            if (xin == 0) c = joint ? SH : LT;                            // lit left edge
            else if (xin == w - 1) c = DK;                                 // shaded right edge, a dark gap to the next column
            else if (joint) c = SH;                                        // a joint between two blocks
            else if (xin == w - 2 && w >= 4) c = lowRows ? SH : (Y + off) % seg == 1 ? SH : c;  // the right flank a shade darker near the gap
            else if ((hash2(X, Y, 6211u) & 31u) == 0) c = LT;              // a fleck
            im.set(x, y, c);
        }
    if (cap)
        for (int x = 0; x < TS; x++) {
            const float X = (float)(tx * TS + x);
            int d = 5 + (int)lroundf(1.6f * sinf(X * 0.83f + 0.7f) + 1.0f * sinf(X * 2.1f + 2.3f));
            d = std::max(3, std::min(8, d));
            for (int y = 0; y < d; y++) im.set(x, y, y < 2 ? HEX(0xffffff) : HEX(0xeef4fa));
            im.set(x, d, HEX(0xb9cde2));
            if (d + 1 < TS) im.set(x, d + 1, HEX(0x3c4660));
        }
    return im;
}

const Spr* Game::snowFaceSpr(int tx, int ty, bool cap, int cfg) {
    const uint64_t key = (uint64_t)(ty * MW + tx) | (uint64_t)(cfg & 255) << 24 | (uint64_t)(cap ? 1 : 0) << 32;
    if (auto it = snowFaceCache.find(key); it != snowFaceCache.end()) return &it->second;
    FastNoiseLite cell((int)(world.seed * 2654435761u + 0x3a91u));
    cell.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    cell.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    cell.SetCellularReturnType(FastNoiseLite::CellularReturnType_CellValue);
    cell.SetFrequency(1.f);
    const Img base = snowFaceBase(cell, tx, ty, cap);
    const int prof = (int)(hash2(tx, ty, 6203u) & 3u);
    return &snowFaceCache.emplace(key, makeSpr(ren, bakeCliff(base, cfg, prof, CLIFF_ICE, 6), 0, 0)).first->second;  // corners rounded a little more than the old 4 px
}

SDL_Texture* Game::snowDetailChunk(int cx, int cy) {
    if (snowDetSeed != world.seed) {  // another world: the old drifts are no use
        freeSnowDetail();
        snowDetSeed = world.seed;
    }
    const int key = cy * 4096 + cx;
    if (auto it = snowDetTex.find(key); it != snowDetTex.end()) return it->second;
    constexpr int P = 8 * TS;
    FastNoiseLite drift, ripple, warp, dash;
    drift.SetSeed((int)(world.seed * 2654435761u + 0x51f0u));
    drift.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    drift.SetFractalType(FastNoiseLite::FractalType_FBm);
    drift.SetFractalOctaves(2);
    drift.SetFrequency(1.f);
    ripple.SetSeed((int)(world.seed * 2246822519u + 0x77c1u));
    ripple.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    ripple.SetFrequency(1.f);
    dash.SetSeed((int)(world.seed * 668265263u + 0x1d35u));
    dash.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    dash.SetFrequency(1.f);
    warp.SetSeed((int)(world.seed * 3266489917u + 0x2bd3u));
    warp.SetDomainWarpType(FastNoiseLite::DomainWarpType_OpenSimplex2);
    warp.SetDomainWarpAmp(26.f);
    warp.SetFrequency(0.012f);
    static const int bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    Img im(P, P);
    // the pale crest of a ripple at a world pixel, 0 = none .. 1 = full: the drifts decide where ripples may form, the strongest in a drift's heart; a second slow
    // field stretched along the wind leaves gaps, so a crest is a run of dashes with soft ends and not one long line
    auto crest = [&](float wx, float wy, float broad) -> float {
        if (broad < -0.10f) return 0.f;
        if (fabsf(ripple.GetNoise(wx * 0.050f, wy * 0.21f)) >= 0.045f) return 0.f;
        const float brk = clampf((dash.GetNoise(wx * 0.10f, wy * 0.45f) + 0.12f) / 0.22f, 0.f, 1.f);
        if (brk <= 0.f) return 0.f;
        return brk * (0.4f + 0.6f * clampf((broad + 0.10f) / 0.40f, 0.f, 1.f));
    };
    for (int y = 0; y < P; y++)
        for (int x = 0; x < P; x++) {
            const float X = (float)(cx * P + x), Y = (float)(cy * P + y);
            float wx = X, wy = Y;
            warp.DomainWarp(wx, wy);
            const float broad = drift.GetNoise(wx * 0.011f, wy * 0.034f);
            const float b = (bayer[y & 3][x & 3] + 0.5f) / 16.f;
            u32 c = 0;
            // contoured, not noisy: flat tones inside a drift, the dither only along its edge (a band ~0.07 wide)
            const float sh = (-broad - 0.16f) / 0.07f, lt = (broad - 0.34f) / 0.07f;
            if (sh > 0.f) c = (sh > b || sh > 1.f) ? (-broad > 0.40f ? RGBA(138, 166, 208, 62) : RGBA(142, 170, 212, 38)) : 0u;
            else if (lt > 0.f) c = (lt > b || lt > 1.f) ? RGBA(255, 255, 255, 105) : 0u;
            if (const float cr = crest(wx, wy, broad); cr > 0.03f) c = RGBA(255, 255, 255, (int)(150.f * cr + 0.5f));
            else {  // a ripple's shadow: the pixel under a crest
                float ux = X, uy = Y - 1.f;
                warp.DomainWarp(ux, uy);
                if (const float cs = crest(ux, uy, drift.GetNoise(ux * 0.011f, uy * 0.034f)); cs > 0.03f) c = RGBA(112, 142, 194, (int)(95.f * cs + 0.5f));
            }
            im.set(x, y, c);
        }
    SDL_Texture* t = toTex(ren, im);
    snowDetTex[key] = t;
    return t;
}

void Game::renderWorld(bool) {
    if (world.mapId == B_SNOW && snowDetSeed != world.seed) { freeSnowDetail(); snowDetSeed = world.seed; }  // another world: the baked drifts and faces are no use
    const float snowDl = daylight();
    const bool snowDet = world.mapId == B_SNOW && interior < 0 && delve < 0 && (world.genOff & GEN_V2_SNOW) && world.tune.snowDetail > 0.01f;
    int tx0 = std::max(0, camX / TS), ty0 = std::max(0, camY / TS);
    int tx1 = std::min(MW - 1, (camX + LW) / TS), ty1 = std::min(MH - 1, (camY + LH) / TS);
    int wf = (int)(timeAcc * 2.4f);
    // wet = water of any depth (a lake of any depth or a brook); off the map counts as water
    auto wetT = [&](int x, int y) { return x < 0 || y < 0 || x >= MW || y >= MH || tileLake(world.tile[y][x]) || world.tile[y][x] == T_SHALLOW; };
    // water: WaterArt (sprites_forest.hpp) bakes every tile from the depth of its neighbourhood: the author's ripples in four depths of light blue,
    // foam and a ragged waterline where it meets land. Depth follows the game's own: a brook / shallows are light, T_LAKE_MID is the author's
    // normal blue, T_WATER (deep) is the deepest; the water in between blends smoothly.
    const int waterFrame = (int)(timeAcc * 1.5f) & (WATER_FRAMES - 1);
    auto depthAt = [&](int x, int y) -> float {
        if (x < 0 || y < 0 || x >= MW || y >= MH) return 3.4f;
        switch (world.tile[y][x]) {
            case T_SHALLOW: return 0.5f;
            case T_LAKE_SHALLOW: return 0.7f;
            case T_LAKE_MID: return 1.75f;
            case T_WATER: return 3.4f;
            default: return 0.f;  // dry land
        }
    };
    static const int NDX[8] = {0, 1, 1, 1, 0, -1, -1, -1}, NDY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};  // N, NE, E, SE, S, SW, W, NW
    auto waterTile = [&](int tx, int ty, int sx, int sy) {
        float dep[3][3];
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < 3; i++) dep[j][i] = depthAt(tx + i - 1, ty + j - 1);
        int land = 0;  // neighbours that are dry land
        for (int k = 0; k < 8; k++)
            if (!wetT(tx + NDX[k], ty + NDY[k])) land |= 1 << k;
        if (land) sprS(A.bankWet[hash2(tx - COX, ty - COY, 5) % 3], sx, sy);  // the wet bank is the bed the shallows show through
        sprS(*A.water.get(ren, tx, ty, waterFrame, dep, land, world.tile[ty][tx] == T_SHALLOW), sx, sy);
    };
    // the bank round the water: dry ground touching water gets wet earth fading out into it (WaterArt::bakeBank)
    auto bankOverlay = [&](int tx, int ty, int sx, int sy) {
        const uint8_t tt = world.tile[ty][tx];
        if (tt != T_GRASS && tt != T_DARK && tt != T_LEAF && tt != T_NEEDLES && tt != T_MOSS && tt != T_ROOTS && tt != T_DIRT && tt != T_SAND && tt != T_MUD) return;
        int wb = 0;
        for (int k = 0; k < 8; k++) {
            const int x = tx + NDX[k], y = ty + NDY[k];
            if (x >= 0 && y >= 0 && x < MW && y < MH && (tileLake(world.tile[y][x]) || world.tile[y][x] == T_SHALLOW)) wb |= 1 << k;
        }
        if (wb) sprS(*A.water.getBank(ren, tx, ty, wb), sx, sy);
    };
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++) {
            int sx = tx * TS - camX, sy = ty * TS - camY;
            uint32_t h = hash2(tx - COX, ty - COY, 5);  // home-relative, so the original area keeps its exact tile variants
            switch (world.tile[ty][tx]) {
                case T_GRASS: case T_DARK: {  // flat pack-style grass: light and medium meadows, dark grass; soil patches show through in some areas
                    const int shade = world.tile[ty][tx] == T_DARK ? 2 : (hash2(tx / 7, ty / 7, 31) & 1);
                    sprS(A.grassT[shade][(h >> 3) % GRASS_VARIANTS], sx, sy);
                    if (soilPatchAt(world, tx, ty)) sprS(A.soilPatch[(h >> 7) % SOIL_PATCHES], sx + (int)((h >> 11) % 3) - 1, sy + (int)((h >> 13) % 3) - 1);
                    break;
                }
                case T_LEAF: sprS(A.leaf[h % 3], sx, sy); break;
                case T_NEEDLES: sprS(A.needles[h % 3], sx, sy); break;
                case T_MOSS: sprS(A.moss[h % 3], sx, sy); break;
                case T_ROOTS: sprS(A.needles[h % 3], sx, sy); break;
                case T_BANK: sprS((wetT(tx, ty - 1) || wetT(tx, ty + 1) || wetT(tx - 1, ty) || wetT(tx + 1, ty) ? A.bankWet : A.bank)[h % 3], sx, sy); break;  // wet earth right at the waterline
                case T_SHALLOW: waterTile(tx, ty, sx, sy); break;
                case T_DIRT: {  // patches of light, medium and dark soil; a few tiles are rocky
                    const int tone = (int)((hash2(tx / 6, ty / 6, 11) + ((h % 7) == 0 ? 1u : 0u)) % 3);
                    sprS((h % 9) == 0 ? A.dirtRock[tone][(h >> 4) & 1] : A.dirtTone[tone][(h >> 3) & 3], sx, sy);
                    break;
                }
                case T_SAND: sprS(A.B.sand[h % 3], sx, sy); break;
                case T_DUNE: sprS(A.B.dune[h % 2], sx, sy); break;
                case T_MUD: sprS(A.B.mud[h % 2], sx, sy); break;
                case T_MURK: sprS(A.B.murk[(wf + (hash2((tx - COX) / 2, (ty - COY) / 2, 9) & 3)) & 3], sx, sy); break;
                case T_BOG: {
                    sprS(A.B.bog[(wf + (hash2((tx - COX) / 2, (ty - COY) / 2, 9) & 3)) & 3], sx, sy);
                    u32 rim = RGBA(150, 190, 150, 70);
                    if (!world.water(tx, ty - 1)) fillRect(sx, sy, TS, 1, rim);
                    if (!world.water(tx, ty + 1)) fillRect(sx, sy + TS - 1, TS, 1, rim);
                    if (!world.water(tx - 1, ty)) fillRect(sx, sy, 1, TS, rim);
                    if (!world.water(tx + 1, ty)) fillRect(sx + TS - 1, sy, 1, TS, rim);
                    break;
                }
                case T_SNOW: case T_SNOWP: {
                    const bool packed = world.tile[ty][tx] == T_SNOWP;
                    if (packed) sprS(A.B.snowp[h % 2], sx, sy);
                    else sprS(A.B.snow[h % 3], sx, sy);
                    if (snowDet) {
                        if (SDL_Texture* dt = snowDetailChunk(tx >> 3, ty >> 3)) {
                            const SDL_Rect src{(tx & 7) * TS, (ty & 7) * TS, TS, TS}, dst{sx, sy, TS, TS};
                            SDL_SetTextureAlphaMod(dt, (Uint8)(clampf(world.tune.snowDetail * (packed ? 0.6f : 1.f) * snowFxK(world, tx, ty), 0.f, 1.f) * 255));
                            SDL_SetTextureColorMod(dt, (Uint8)(150 + 105 * snowDl), (Uint8)(172 + 83 * snowDl), (Uint8)(236 + 19 * snowDl));  // pale blue at night
                            SDL_RenderCopy(ren, dt, &src, &dst);
                        }
                    }
                    if (world.plat[ty][tx] == 1) {  // raised ground: a clean, paler top (the ground's speckle fades), and the lip along every side that looks onto plain ground
                        fillRect(sx, sy, TS, TS, RGBA(255, 255, 255, 105));
                        static const int LX[8] = {0, 1, 0, -1, 1, 1, -1, -1}, LY[8] = {-1, 0, 1, 0, -1, 1, 1, -1};  // N E S W NE SE SW NW
                        int lc = 0;
                        for (int d = 0; d < 8; d++) {
                            const int nx = tx + LX[d], ny = ty + LY[d];
                            if (nx >= 0 && ny >= 0 && nx < MW && ny < MH && world.plat[ny][nx] == 0 && (world.tile[ny][nx] != T_CLIFF || d == 1 || d == 3)) lc |= 1 << d;  // (the side of a top that runs beside the end of a wall has its rim too)
                        }
                        if (lc & (1 | 8)) lc &= ~128;
                        if (lc & (1 | 2)) lc &= ~16;
                        if (lc & (2 | 4)) lc &= ~32;
                        if (lc & (4 | 8)) lc &= ~64;
                        if (lc) sprS(*A.blend.lip(ren, lc, (int)(hash2(tx, ty, 6207u) & 3u)), sx, sy);
                    }
                    break;
                }
                case T_ICE: {
                    sprS(A.B.ice[h % 2], sx, sy);
                    if (((int)(timeAcc * 1.2f) + (h & 7)) % 9 == 0) fillRect(sx + (h >> 4) % 12 + 2, sy + (h >> 8) % 12 + 2, 2, 1, RGBA(255, 255, 255, 200));  // glints
                    break;
                }
                case T_STONE: sprS(A.B.stone[h % 3], sx, sy); break;
                case T_COBBLE: sprS(A.T.cobble[h % 3], sx, sy); break;
                case T_VOID: break;  // outside the room: the canvas is cleared to black first
                // ---- the delves (sprites_delve.hpp)
                case T_CFLOOR: sprS(A.D.cfloor[h % 4], sx, sy); break;
                case T_IFLOOR: sprS(A.D.ifloor[h % 4], sx, sy); break;
                case T_BRICK: sprS(A.D.brick[h % 3], sx, sy); break;
                case T_CARPET: sprS(A.D.carpet[(tx + ty) & 1], sx, sy); break;
                case T_RAIL:
                    sprS(A.D.cfloor[h % 4], sx, sy);
                    sprS(A.D.rail[(world.tileIs(tx, ty - 1, T_RAIL) || world.tileIs(tx, ty + 1, T_RAIL)) ? 1 : 0], sx, sy);
                    break;
                case T_CWALL: case T_IWALL: {
                    const uint8_t below = ty + 1 < MH ? world.tile[ty + 1][tx] : (uint8_t)T_VOID;
                    const bool face = below != T_CWALL && below != T_IWALL && below != T_VOID;  // the wall seen from the front where floor lies below it
                    if (world.tile[ty][tx] == T_CWALL) sprS(face ? A.D.cwallFace[h % 3] : A.D.cwallTop[h % 3], sx, sy);
                    else sprS(face ? A.D.iwallFace[h % 3] : A.D.iwallTop[h % 3], sx, sy);
                    break;
                }
                case T_WALL: sprS(A.T.wallFace[std::max(0, interior)], sx, sy); break;
                case T_WALLT: sprS(A.T.wallCap[std::max(0, interior)], sx, sy); break;
                case T_FLOORW: sprS(A.T.floorW[(tx * 3 + ty) % 3], sx, sy); break;
                case T_FLOORS: sprS(A.T.floorS[(tx + ty * 2) % 3], sx, sy); break;
                case T_FLOORT: sprS(A.T.floorT[(tx + ty) & 1], sx, sy); break;
                case T_RUG: sprS(A.T.rug[std::max(0, interior)], sx, sy); break;
                case T_MAT: sprS(A.T.mat, sx, sy); break;
                case T_CLIFF: {
                    if (world.mapId == B_SNOW) {  // snow-capped brown rock: the top tile of a ridge carries the snow, the tile below it is bare face
                        // the sides facing open ground are finished off by the 8-neighbour mask (art/tile_edges.hpp, bakeCliff): foot, lit / shaded
                        // side edges, rounded and notched corners. What is cut away shows the snow drawn under the tile.
                        static const int CX[8] = {0, 1, 0, -1, 1, 1, -1, -1}, CY[8] = {-1, 0, 1, 0, -1, 1, 1, -1};  // N E S W NE SE SW NW
                        int cfg = 0;
                        for (int d = 0; d < 8; d++) {
                            const int nx = tx + CX[d], ny = ty + CY[d];
                            if (nx >= 0 && ny >= 0 && nx < MW && ny < MH && world.tile[ny][nx] != T_CLIFF) cfg |= 1 << d;  // the map's edge counts as rock
                        }
                        // where the wall ends against the plateau's own top (the top reaches further south beside it) the face runs flush into it: no cut end,
                        // no outline; the top's rim (below) carries on beside it, so wall and top join up
                        if (tx > 0 && world.plat[ty][tx - 1] == 1) cfg &= ~(8 | 64 | 128);
                        if (tx + 1 < MW && world.plat[ty][tx + 1] == 1) cfg &= ~(2 | 16 | 32);
                        if (cfg & (1 | 8)) cfg &= ~128;   // a diagonal only matters where both its sides are rock
                        if (cfg & (1 | 2)) cfg &= ~16;
                        if (cfg & (2 | 4)) cfg &= ~32;
                        if (cfg & (4 | 8)) cfg &= ~64;
                        if (const Spr* cs = snowFaceSpr(tx, ty, (cfg & 1) != 0, cfg)) {
                            if (cfg) sprS(A.B.snow[h % 3], sx, sy);  // the ground under the rounded and notched corners
                            sprS(*cs, sx, sy);
                        }
                        break;
                    }
                    sprS(A.B.cliff[h % 3], sx, sy);
                    if (!world.tileIs(tx, ty - 1, T_CLIFF)) fillRect(sx, sy, TS, 2, HEX(0x9b9eb2));
                    if (!world.tileIs(tx, ty + 1, T_CLIFF)) fillRect(sx, sy + TS - 3, TS, 3, HEX(0x2c2d3a));
                    break;
                }
                case T_WATER: case T_LAKE_MID: case T_LAKE_SHALLOW: waterTile(tx, ty, sx, sy); break;
            }
            if (const int bc = blendClass(world.tile[ty][tx]); bc >= 0) blendEdges(tx, ty, sx, sy, bc, h);  // soft borders (tile_edges.hpp)
            bankOverlay(tx, ty, sx, sy);
            if (ty > 0 && !world.solid[ty][tx]) {  // ground standing higher than the tile below it: its face shows along the top of that tile
                const int rk = tileRank(world.tile[ty][tx]), upRk = tileRank(world.tile[ty - 1][tx]);
                if (rk >= 0 && upRk > rk) sprS(A.lip[tileLipStyle(world.tile[ty - 1][tx])][(tx * 7 + ty * 3) & 3], sx, sy);
            }
            if (wet > 0.02f && wetGroundTile(world.tile[ty][tx])) fillRect(sx, sy, TS, TS, RGBA(24, 34, 56, (int)(55 * wet)));  // rain-soaked ground is darker
        }
    if (world.mapId == B_SNOW && interior < 0 && delve < 0) drawSnowDrift();  // snow streaming low across the open ground
    drawFootprints();  // what walking left behind (game_prints.cpp)
    drawTracks();      // animal tracks the hunter can read (game_tracks.cpp)
    drawBossTelegraph();
    // blood stains
    if (inHomestead()) drawHomesteadGround();
    for (auto& d : decals) {
        float a = std::min(1.f, (d.life - d.t) / 8.f) * 0.9f;
        sprW(A.blood[d.size], d.p.x, d.p.y, false, a);
    }
    // thrown weapons lying on the ground
    for (auto& it : items) {
        const float left = (it.item >= 0 ? DROP_LIFE : THROWN_LIFE) - it.t;
        if (left < DESPAWN_WARN && despawnBlink(timeAcc + it.pos.x * 0.01f)) continue;  // about to vanish: blinks
        if (it.item >= 0) { sprW(A.shadowSoft, it.pos.x, it.pos.y, false, 0.6f); icon(itemIcon(it.item), (int)it.pos.x - camX, (int)it.pos.y - camY - 4, DROP_ICON); }  // a stack the hunter dropped: a small icon, not a whole tile
        else sprW(it.ammo >= 0 ? A.ammoWorld[it.ammo] : A.wpnWorld[it.wpn], it.pos.x, it.pos.y - 2, false, 1.f, it.angle * 57.2958, true);
        float g = fmodf(timeAcc + it.pos.x * 0.01f, 1.8f);
        if (g < 0.25f) {
            int gx = (int)it.pos.x - camX + 3, gy = (int)it.pos.y - camY - 6;
            fillRect(gx, gy - 1, 1, 3, RGBA(255, 255, 255, 220));
            fillRect(gx - 1, gy, 3, 1, RGBA(255, 255, 255, 220));
        }
    }
    if (delve >= 0) drawDelveFloor();
    // carcasses
    for (auto& a : animals) {
        if (!a.dead) continue;
        if (a.pos.x < camX - 50 || a.pos.x > camX + LW + 50 || a.pos.y < camY - 50 || a.pos.y > camY + LH + 50) continue;
        const int bi = a.variant + 2 * a.tier;
        const AnimalClips* dc = animalClips(a.kind);
        const RichAnimal* rb = A.rich[a.kind].get();
        const bool vert = !dc && (a.face8 == F_S || a.face8 == F_N) && A.animalNS[a.kind][bi][0][0].ok();
        // species with a full set lie in the last pose of their death clip; the crawlers use their dead sprite (a mirror for west)
        const Spr& s = (dc && rb) ? rb->s[bi][std::max(0, std::min((int)AV_VIEWS - 1, a.face8))][AN_DEATH][dc->poses[AN_DEATH] - 1]
                       : (vert ? A.animalNS[a.kind][bi][a.face8 == F_N][4] : A.animal[a.kind][bi][4]);
        const bool flipC = !(dc && rb) && !vert && (a.face8 == F_W || (a.face8 != F_E && a.face < 0));
        const float corpseLeft = (a.tier == TR_LEGENDARY ? CORPSE_LIFE_LEGEND : CORPSE_LIFE) - a.deadT;
        const float blink = (a.dead && a.fade >= 1.f && corpseLeft < DESPAWN_WARN && despawnBlink(timeAcc)) ? 0.45f : 1.f;  // about to vanish: pulses
        sprW(s, a.pos.x, a.pos.y - ((dc && rb) ? 0 : 6), flipC, a.fade * blink);
        if (a.flash > 0) {
            Spr f = s;
            f.t = s.flash;
            sprW(f, a.pos.x, a.pos.y - ((dc && rb) ? 0 : 6), flipC, a.flash / 0.16f * 0.8f);
        }
        if (a.totalRemain > 0 && fmodf(timeAcc * 0.8f + a.pos.x, 2.f) < 0.3f) {
            int gx = (int)a.pos.x - camX + 4, gy = (int)a.pos.y - camY - 10;
            fillRect(gx, gy - 2, 1, 5, RGBA(255, 240, 160, 230));
            fillRect(gx - 2, gy, 5, 1, RGBA(255, 240, 160, 230));
        }
    }
    // decor (swaying grass and flowers)
    // decor, props and nodes are sorted by y, so only the rows near the camera are visited
    auto yLess = [](const auto& e, float y) { return e.y < y; };
    // a bush right in front of the hunter is drawn again over him (below, in the y-sorted layer), so he stands half hidden behind it
    struct FrontBush { const Spr *under, *s; float x, y, alpha, mainA, ang, sqx, sqy, sqr; bool squash; };
    std::vector<FrontBush> frontBush;
    for (auto it = std::lower_bound(world.decor.begin(), world.decor.end(), camY - 10.f, yLess); it != world.decor.end() && it->y <= camY + LH + 30; ++it) {
        const Decor& d = *it;
        if (d.x < camX - 20 || d.x > camX + LW + 20) continue;
        const Spr* s = nullptr;
        bool sway = true;
        float swayAmt = 6.f, alpha = 1.f;
        bool squash = false;  // round bushes: no idle sway, a springy squash when brushed
        bool bushy = false;   // a bush (bush_react.hpp): shaken, shed over, and drawn in front of the hunter when he stands behind it
        const Spr* under = nullptr;  // a snow shrub that has dropped its snow: its bare twigs, with the snow fading back in over them
        float mainA = 1.f;
        switch (d.kind) {
            case D_FLOWER0: case D_FLOWER1: case D_FLOWER2: s = &A.flower[d.kind]; break;
            case D_TUFT: s = &A.tuft; break;
            case D_ICEHOLE: s = &A.iceHole; sway = false; break;  // a seal's breathing hole in the ice
            case D_FISHHOLE: s = &A.B.fishHole; sway = false; break;  // the Ice Fisher's fishing hole
            case D_DEN: s = &A.den; sway = false; break;           // an arctic fox's den in a snowdrift
            case D_BLADES: s = &A.blades; swayAmt = 3.f; break;  // tall blade grass: a gentle sway, and it leans away from whoever runs through it
            case D_MUSHROOM: s = &A.mushroom; sway = false; break;
            case D_PEBBLES: s = &A.pebbleSet[rockTheme()][(std::abs((int)(d.x / 9.f)) + 3 * std::abs((int)(d.y / 9.f))) % ROCK_LOOKS]; sway = false; break;
            case D_BUSH0: case D_BUSH1: {
                s = world.mapId == B_HOMESTEAD ? (d.kind == D_BUSH1 ? &A.homeBush : &A.bush[0]) : rockTheme() == B_SWAMP ? &A.B.swampBush[d.kind - D_BUSH0] : rockTheme() == B_SNOW ? &A.B.snowBush[d.kind - D_BUSH0] : rockTheme() == B_MOUNTAIN ? &A.B.mountainBush[d.kind - D_BUSH0] : &A.bush[d.kind - D_BUSH0];
                // twig and frond bushes stir in the wind and lean away from whoever runs through them; the round ones stay put and
                // squash a little when brushed (see below)
                const int bv = d.kind - D_BUSH0, bt = world.mapId == B_HOMESTEAD ? -1 : rockTheme();
                const bool twiggy = bt == B_SWAMP ? bv == 1 : bt == B_SNOW ? true : bt == B_MOUNTAIN ? bv == 1 : false;
                sway = twiggy;
                squash = !twiggy;
                swayAmt = bt == B_SWAMP ? 3.5f : 2.5f;
                bushy = true;
                if (bt == B_SNOW) {
                    const float k = bushSnowK((size_t)(it - world.decor.begin()));
                    if (k < 1.f) under = &A.B.snowBushBare[bv], mainA = k;
                }
                break;
            }
            case D_DRYBUSH: s = &A.B.drybush; swayAmt = 3.f; bushy = true; break;
            case D_SKULL: s = &A.B.skull; sway = false; break;
            case D_DROCK0: case D_DROCK1: case D_DROCK2: case D_DROCK3: s = &A.desertDecor[d.kind - D_DROCK0]; sway = false; break;
            case D_SANDTUFT: s = &A.B.sandtuft; swayAmt = 1.5f; break;  // the agave: stiff leaves, so only a hint of sway
            case D_REEDS0: case D_REEDS1: s = &A.B.reeds[d.kind - D_REEDS0]; swayAmt = 4.f; break;
            case D_LILY: s = &A.B.lily; sway = false; break;
            case D_BUBBLES: s = &A.B.bubbles; sway = false; alpha = fmaxf(0.f, sinf(timeAcc * 1.3f + d.ph * 3.f)) * 0.9f + 0.1f; break;
            case D_DRIFT: s = &A.B.drift; sway = false; break;
            case D_FROSTCRACK: s = &A.B.frostCrack[((int)(d.ph * 7.f)) & 3]; sway = false; break;
            case D_YOUNGPINE: s = &A.B.youngPine; swayAmt = 2.5f; break;
            case D_TRACKS: s = &A.B.tracks[((int)(d.ph * 7.f)) & 3]; sway = false; break;
            case D_BURIEDROCK: s = &A.B.buriedRock[((int)(d.ph * 7.f)) & 3]; sway = false; break;
            case D_FROSTTUFT: s = &A.B.frosttuft; swayAmt = 3.f; break;
            case D_ALPINE: s = &A.B.alpine; break;
            case D_STUMP: s = world.mapId == B_SNOW ? &A.B.snowStump[((int)(d.ph * 7.f)) % 3] : &A.R.stump; sway = false; break;
            case D_VIOLET: case D_POPPY: s = &A.flowerX[d.kind - D_VIOLET]; break;
            default: s = &A.B.gravel; sway = false; break;
        }
        float ang = sway ? sinf(windT * 1.9f + d.ph + d.x * 0.02f) * swayAmt * (0.8f + wind) + windDir.x * swayAmt * 0.6f * clampf(wind, 0.f, 2.f) * (0.5f + windGust) + grassBend((size_t)(it - world.decor.begin())) : 0.f;  // plus the lean from whoever just walked through
        float sqx = 1.f, sqy = 1.f, sqr = 0.f;
        if (squash) {  // the bend is the lean a walker's kick would give a tuft: a bush turns it into a short squat-and-spring and a slight tilt
            const float bend = grassBend((size_t)(it - world.decor.begin())), k = std::min(1.f, fabsf(bend) / 30.f);
            sqx = 1.f + 0.10f * k, sqy = 1.f - 0.12f * k, sqr = bend * 0.12f;
        }
        auto putBush = [&](const Spr& sp, float a) {
            if (squash) sprWSquash(sp, d.x, d.y, sqx, sqy, sqr, a);
            else sprW(sp, d.x, d.y, false, a, ang, true);
        };
        if (under) putBush(*under, alpha);
        if (mainA > 0.f) putBush(*s, alpha * mainA);
        if (bushy && interior < 0 && !p.dead && d.y > p.pos.y + 1.f && d.y < p.pos.y + 14.f && fabsf(d.x - p.pos.x) < 14.f)
            frontBush.push_back({under, s, d.x, d.y, alpha, mainA, ang, sqx, sqy, sqr, squash});
    }
    // resource nodes: a soft shadow, the plant or ore, and a slow glint so they read as "pick me"
    for (auto it = std::lower_bound(world.nodes.begin(), world.nodes.end(), camY - 10.f, yLess); it != world.nodes.end() && it->y <= camY + LH + 30; ++it) {
        const ResNode& n = *it;
        if (n.x < camX - 20 || n.x > camX + LW + 20) continue;
        const HState& hs = nodeSt[it - world.nodes.begin()];
        if (hs.gone) {  // finished: a stub of the plant, or the broken-up rock, until it grows back
            const bool plantRes = RES[nodeRes(n.kind)].cls == RCL_PLANT;
            const Spr& left = !plantRes && A.oreLeft[n.kind][0].t ? A.oreLeft[n.kind][remainsVariant(n.x, n.y)] : (A.R.stub.t && plantRes ? A.R.stub : A.R.rubble);
            sprW(left, n.x, n.y);
            continue;
        }
        const float grow = hs.fade > 0 ? 1.f - hs.fade / REGROW_FADE : 1.f;  // regrowing: fades back in
        const float shk = hs.shake > 0 ? sinf(timeAcc * 70.f) * hs.shake * 9.f : 0.f;
        bool plant = n.kind != RN_ORE && n.kind != RN_CACTUS && n.kind != RN_COPPER && n.kind != RN_GOLD && n.kind != RN_AMBER && n.kind != RN_VOIDSTONE && (n.kind < RN_COAL || n.kind >= RN_BERRY);
        // the ore nodes are rocks with the metal in their cracks, drawn in the stone of the biome they stand in (mines and delves use the mountain's dark stone)
        const int om = n.kind == RN_COPPER ? OREM_COPPER : n.kind == RN_ORE ? OREM_IRON : n.kind == RN_SILVER ? OREM_SILVER : n.kind == RN_GOLD ? OREM_GOLD : n.kind == RN_COAL ? OREM_COAL : n.kind == RN_AMBER ? OREM_AMBER : -1;
        sprW(om >= 0 ? A.oreRock[om][world.isDelve() ? B_MOUNTAIN : rockTheme()] : n.kind >= RN_VOIDSTONE ? A.D.node[5 + n.kind - RN_VOIDSTONE] : n.kind >= RN_BERRY ? A.H.node[n.kind - RN_BERRY] : n.kind >= RN_COAL ? A.D.node[n.kind - RN_COAL] : A.B.node[n.kind], n.x + shk, n.y, false, std::max(0.25f, grow), plant ? sinf(timeAcc * 1.6f + n.ph) * 3.f : 0.f, true);
        float g = fmodf(timeAcc * 0.9f + n.ph, 3.2f);
        if (hs.fade <= 0 && g < 0.22f) {
            int gx = (int)n.x - camX + 4, gy = (int)n.y - camY - 12;
            fillRect(gx, gy - 1, 1, 3, RGBA(255, 255, 255, 220));
            fillRect(gx - 1, gy, 3, 1, RGBA(255, 255, 255, 220));
        }
    }

    // ---- y-sorted layer
    struct DL {
        float y;
        int t, i;
        int j = 0;
    };
    std::vector<DL> dl;
    // (the Frozen Fortress is sorted at its door but hangs 96 px below it, so the range starts well above the screen)
    for (auto it = std::lower_bound(world.props.begin(), world.props.end(), camY - 110.f, yLess); it != world.props.end() && it->y <= camY + LH + 90; ++it) {
        if (it->x < camX - 80 || it->x > camX + LW + 80) continue;
        dl.push_back({it->y, 0, (int)(it - world.props.begin())});
    }
    for (size_t i = 0; i < structs.size(); i++) dl.push_back({structs[i].pos().y, 5, (int)i});
    dl.push_back({world.keeper.y, 1, 0});
    dl.push_back({world.questGiver.y, 4, 0});
    for (size_t i = 0; i < world.npcs.size(); i++) dl.push_back({world.npcs[i].pos.y, 6, (int)i});
    if (world.mapId == B_HOME && interior < 0)
        for (size_t i = 0; i < villagers.size(); i++) dl.push_back({villagers[i].pos.y, 12, (int)i});
    dl.push_back({p.pos.y, 2, 0});
    for (size_t i = 0; i < frontBush.size(); i++) dl.push_back({frontBush[i].y, 13, (int)i});
    if (delve >= 0) {
        for (size_t i = 0; i < mons.size(); i++) dl.push_back({mons[i].pos.y - (mons[i].state == MN_DEAD ? 4.f : 0.f), 7, (int)i});
        for (size_t i = 0; i < dchests.size(); i++) dl.push_back({dchests[i].pos.y, 8, (int)i});
    }
    for (size_t i = 0; i < animals.size(); i++)
        if (!animals[i].dead) dl.push_back({animals[i].pos.y, 3, (int)i});
    if (inHomestead()) {
        for (size_t i = 0; i < this->hs.pieces.size(); i++) {
            const HPiece& pc = this->hs.pieces[i];
            const HPieceDef& D = HPDEF[pc.kind];
            if (!(pc.tx + pc.w() >= camX / TS - 2 && pc.tx <= (camX + LW) / TS + 2 && pc.ty + pc.h() >= camY / TS - 3 && pc.ty <= (camY + LH) / TS + 5)) continue;
            if (pc.kind == HP_PLOT) {
                if (pc.crop >= 0) dl.push_back({pc.ty * (float)TS + 12.f, 10, (int)i});
            } else if (pc.kind == HP_PEN) {
                for (int y = pc.ty; y < pc.ty + pc.h(); y++)
                    for (int x = pc.tx; x < pc.tx + pc.w(); x++)
                        if (x == pc.tx || y == pc.ty || x == pc.tx + pc.w() - 1 || y == pc.ty + pc.h() - 1) dl.push_back({(y + 1) * (float)TS, 9, (int)i, 1 + y * MW + x});
            } else if (D.layer == HL_OBJECT)
                dl.push_back({(pc.ty + pc.h()) * (float)TS - (pc.kind == HP_GREENHOUSE ? 6.f : 0.f), 9, (int)i});
        }
        for (size_t i = 0; i < this->hs.animals.size(); i++) dl.push_back({this->hs.animals[i].py, 11, (int)i});
    }
    std::sort(dl.begin(), dl.end(), [](const DL& a, const DL& b) { return a.y < b.y; });

    float night = interior >= 0 ? 0.f : 1.f - daylight();
    drawGroundShadows();  // tree and rock shadows lie on the ground, under everything that stands on it
    drawGroundShadows(1);  // and the shadows of the torch and the fires, at night
    // pebbles scattered round the foot of a small rock or a boulder, like the grass round a tree: which pile and which way round it lies is fixed per rock
    auto pebblesAround = [&](const Prop& pr) {
        const int tx = (int)(pr.x / TS), ty = (int)(pr.y / TS);
        sprWG(A.pebbleSet[rockTheme()][std::abs(tx * 11 + ty * 5) % ROCK_LOOKS], pr.x, pr.y + 2, ((tx + ty) & 1) != 0);
    };
    // grass round the foot of a minable rock on grassland, swaying like the grass round a tree; it stays when the rock is broken
    // trees on leaf litter, needles or bare roots have no grass skirt round their roots
    auto bareFloor = [&](const Prop& pr) {
        const uint8_t gt = world.tileAt(pr.x, pr.y - 1.f);
        return gt == T_LEAF || gt == T_NEEDLES || gt == T_ROOTS;
    };
    auto grassAround = [&](const Prop& pr) {
        const int tx = std::min(MW - 1, std::max(0, (int)(pr.x / TS))), ty = std::min(MH - 1, std::max(0, (int)((pr.y - 1.f) / TS)));
        const uint8_t gt = world.tile[ty][tx];
        const bool frosty = world.mapId == B_SNOW && (gt == T_SNOW || gt == T_SNOWP);  // the snowfield's rocks stand in frosty tufts
        if (gt != T_GRASS && gt != T_DARK && gt != T_MOSS && !frosty) return;  // not on sand, stone, bare forest floors or the interior floors
        const int kind = rockClass(pr.kind);
        const int look = rockLookOf(pr);
        const int fr = ((int)(windT * 5.f + pr.ph * ROCK_GRASS_FRAMES / 6.2832f) % ROCK_GRASS_FRAMES + ROCK_GRASS_FRAMES) % ROCK_GRASS_FRAMES;
        sprWG(frosty ? A.rockFrost[kind][look][fr] : A.rockGrass[kind][look][fr], pr.x, pr.y + 2);
    };
    // frosty tufts round the foot of a snowfield object (sign, tent, lamp, fence, cairn ...): the ring is picked by the object's width
    auto frostFoot = [&](const Prop& pr, int objW) {
        if (world.mapId != B_SNOW || interior >= 0 || delve >= 0) return;
        const int tx = (int)(pr.x / TS), ty = (int)(pr.y / TS);
        const int fr = ((int)(windT * 5.f + (tx * 7 + ty * 3) * 0.7f) % ROCK_GRASS_FRAMES + ROCK_GRASS_FRAMES) % ROCK_GRASS_FRAMES;
        sprWG(A.frostSkirt[frostSkirtFor(objW)][fr], pr.x, pr.y + 2);
    };
    // frosty tufts round a snowfield tree's roots (the spruce set for the pines and birches, the dead set for the dead spruces)
    auto frostRoots = [&](const Prop& pr, bool dead, float lean) {
        if (world.mapId != B_SNOW || interior >= 0 || delve >= 0) return;
        const int fr = ((int)(windT * SNOW_SPRUCE_FPS + pr.ph * GRASS_FRAMES / 6.2832f) % GRASS_FRAMES + GRASS_FRAMES) % GRASS_FRAMES;
        const Spr& g = dead ? A.snowGrassDead[fr] : A.snowGrass[fr];
        sprWG(g, pr.x, pr.y + 1, false, 1.f, lean);
    };
    for (auto& e : dl) {
        if (e.t == 13) {  // a bush the hunter stands behind: over him
            const FrontBush& fb = frontBush[e.i];
            auto put = [&](const Spr& sp, float a) {
                if (fb.squash) sprWSquash(sp, fb.x, fb.y, fb.sqx, fb.sqy, fb.sqr, a);
                else sprW(sp, fb.x, fb.y, false, a, fb.ang, true);
            };
            if (fb.under) put(*fb.under, fb.alpha);
            if (fb.mainA > 0.f) put(*fb.s, fb.alpha * fb.mainA);
            continue;
        }
        if (e.t == 0) {
            const Prop& pr = world.props[e.i];
            const int camSave = camX;
            const int rs = propRsId[e.i];
            if (rs >= 0) {
                const HState& hs = propSt[e.i];
                if (hs.gone || hs.fade > 0) {  // felled tree / broken rock, then a sprout while it grows back
                    const bool tree = RES[rs].cls == RCL_TREE;
                    if (tree) {  // every kind of tree leaves its own stump; the mirror image on every other tile
                        int sk = STUMP_OAK;
                        switch (pr.kind) {
                            case P_OAK0: case P_OAK1: {
                                const int ov = oakVariantFor((int)(pr.x / TS), (int)(pr.y / TS));
                                sk = ov == 3 ? STUMP_OLDOAK : ov == 4 ? STUMP_SAPLING : ov == 5 ? STUMP_DEADOAK : STUMP_OAK;
                            } break;
                            case P_PINE: sk = STUMP_PINE; break;
                            case P_PALM: sk = STUMP_PALM; break;
                            case P_SNOWPINE: sk = STUMP_SNOWPINE; break;
                            case P_DEADTREE0: case P_DEADTREE1: sk = STUMP_DEADSPRUCE; break;
                            case P_BARETREE: sk = snowBirchAt(pr.x, pr.y) ? STUMP_BIRCH : STUMP_SNOWDEAD; break;
                            case P_CACTUS0: case P_CACTUS1: sk = STUMP_CACTUS; break;
                            case P_LOG: sk = STUMP_LOG; break;
                            default: break;
                        }
                        sprW(A.treeStump[sk], pr.x, pr.y, (((int)(pr.x / TS) + (int)(pr.y / TS)) & 1) != 0);
                    } else {
                        const int rv = remainsVariant(pr.x, pr.y);
                        sprW(isRockKind(pr.kind) ? A.rockLeft[rockTheme()][rockClass(pr.kind)][rv] : (pr.kind == P_CRYSTAL ? A.crystalLeft[rv] : (pr.kind == P_ICESPIKE ? A.oreLeft[RN_ETERNALICE][rv] : A.R.rubble)), pr.x, pr.y);
                    }
                    if (!tree && rockClass(pr.kind) >= 1) pebblesAround(pr);  // the pebbles stay where the rock was
                    if (!tree && isRockKind(pr.kind)) grassAround(pr);
                    if (tree && pr.kind == P_SNOWPINE)  // the frosty grass stays around the stump too
                        sprWG(A.snowGrass[((int)(windT * SNOW_SPRUCE_FPS + pr.ph * GRASS_FRAMES / 6.2832f) % GRASS_FRAMES + GRASS_FRAMES) % GRASS_FRAMES], pr.x, pr.y + 1);
                    if (tree && pr.kind == P_BARETREE) frostRoots(pr, !snowBirchAt(pr.x, pr.y), 0.f);  // ... and round a felled birch or dead spruce
                    if (tree && (pr.kind == P_OAK0 || pr.kind == P_OAK1 || pr.kind == P_PINE)) {  // the grass stays around the stump
                        const int gset = pr.kind == P_PINE ? GRASS_SPRUCE : OAK_VAR[oakVariantFor((int)(pr.x / TS), (int)(pr.y / TS))].grass;
                        if (!bareFloor(pr)) sprWG(A.treeGrass[gset][((int)(windT * OAK_FPS + pr.ph * GRASS_FRAMES / 6.2832f) % GRASS_FRAMES + GRASS_FRAMES) % GRASS_FRAMES], pr.x, pr.y + 1);
                    }
                    if (!hs.gone && tree) sprW(A.R.sprout, pr.x, pr.y - 2, false, 0.4f + 0.6f * (1.f - hs.fade / REGROW_FADE));
                    continue;
                }
                if (hs.shake > 0) camX -= (int)(sinf(timeAcc * 70.f) * hs.shake * 9.f);  // the tree shudders / the rock rattles when struck
            }
            switch (pr.kind) {
                case P_OAK0: case P_OAK1:
                    {
                        const int ov = oakVariantFor((int)(pr.x / TS), (int)(pr.y / TS));  // which look this tree has, fixed per tree
                        const int fr = ((int)(windT * OAK_FPS + pr.ph * OAK_FRAMES / 6.2832f) % OAK_FRAMES + OAK_FRAMES) % OAK_FRAMES;
                        sprWLean(A.oak[ov][fr], pr.x, pr.y + 1, windLean(A.oak[ov][fr]));
                        if (!bareFloor(pr)) sprWG(A.treeGrass[OAK_VAR[ov].grass][fr], pr.x, pr.y + 1, false, 1.f, windLean(A.treeGrass[OAK_VAR[ov].grass][fr]));  // the grass around the roots, drawn over the trunk
                    }
                    break;
                case P_PINE:
                    {
                        const int fr = ((int)(windT * PINE_FPS + pr.ph * PINE_FRAMES / 6.2832f) % PINE_FRAMES + PINE_FRAMES) % PINE_FRAMES;
                        sprWLean(A.pine[fr], pr.x, pr.y + 1, windLean(A.pine[fr]));
                        if (!bareFloor(pr)) sprWG(A.treeGrass[GRASS_SPRUCE][fr], pr.x, pr.y + 1, false, 1.f, windLean(A.treeGrass[GRASS_SPRUCE][fr]));  // the grass around the roots, drawn over the trunk
                    }
                    break;
                case P_ROCK0: case P_ROCK1: case P_BOULDER: case P_SANDBOULDER: case P_SANDROCK0: case P_SANDROCK1: case P_SNOWROCK:
                    {  // every biome's rocks are the same pictures in the biome's colours
                        const int rc = rockClass(pr.kind), look = rockLookOf(pr), th = rockTheme();
                        if (rc >= 1) pebblesAround(pr);
                        sprW(rc == 0 ? A.rockTiny[th][look] : rc == 1 ? A.rockSmall[th][look] : A.rockLarge[th][look], pr.x, pr.y);
                        grassAround(pr);
                    }
                    break;
                case P_HUT: sprW(A.hut, pr.x, pr.y); break;
                case P_FIRE: sprW(A.fire[(int)(timeAcc * 9) % 3], pr.x, pr.y); break;
                case P_CRATE: sprW(A.crate, pr.x, pr.y); break;
                case P_BARREL: sprW(A.barrel, pr.x, pr.y); break;
                case P_SIGN: sprW(A.sign, pr.x, pr.y); break;
                // ---- the town
                case P_BLD0: case P_BLD1: case P_BLD2: case P_BLD3: case P_BLD4: case P_BLD5:
                    sprW(A.T.bld[pr.kind - P_BLD0], pr.x, pr.y);
                    break;
                case P_FURN: sprW(A.T.furn[std::max(0, std::min(FU_COUNT - 1, (int)pr.ph))], pr.x, pr.y); break;
                case P_LAMP: sprW(A.T.lamp, pr.x, pr.y); break;
                case P_FENCE: sprW(A.T.fence, pr.x, pr.y); break;
                case P_SNOWMAN: sprW(A.B.snowman[(int)pr.ph & 3], pr.x, pr.y); frostFoot(pr, A.B.snowman[0].w); break;
                case P_SNOWSIGN: { const Spr& ss = A.B.snowSign[std::max(0, std::min(3, (int)pr.ph))]; sprW(ss, pr.x, pr.y); frostFoot(pr, ss.w); } break;
                case P_SNOWLAMP: sprW(A.B.snowLamp, pr.x, pr.y); frostFoot(pr, A.B.snowLamp.w); break;
                case P_SNOWFENCE: sprW(A.B.snowFence, pr.x, pr.y); frostFoot(pr, A.B.snowFence.w); break;
                case P_SNOWCAMPFIRE: sprW(A.B.snowFire[((int)(timeAcc * 9) + (int)pr.ph) % 3], pr.x, pr.y); break;
                case P_SNOWTENT: sprW(A.B.snowTentSnowy, pr.x, pr.y); frostFoot(pr, A.B.snowTentSnowy.w); break;  // the snowfield's tents are the snow-covered ones; the regular tent is for the other biomes' camps
                case P_SNOWLOG: sprW(A.B.snowLog[((int)(pr.x / TS) * 5 + (int)(pr.y / TS) * 3) % 3], pr.x, pr.y); frostFoot(pr, A.B.snowLog[0].w); break;
                case P_LOOKOUT: sprW(A.B.lookout, pr.x, pr.y); frostFoot(pr, A.B.lookout.w); break;
                case P_FISHRACK: sprW(A.B.fishRack, pr.x, pr.y); frostFoot(pr, A.B.fishRack.w); break;
                case P_FIREWOOD: sprW(A.B.firewood, pr.x, pr.y); frostFoot(pr, A.B.firewood.w); break;
                case P_SNOWTENTOLD: sprW(A.B.tentOld[0], pr.x, pr.y); frostFoot(pr, A.B.tentOld[0].w); break;  // the snow-covered one: tentOld[1] (bare) is for the other biomes
                case P_SNOWASH: sprW(A.B.ashRing, pr.x, pr.y); break;
                case P_CAMPFIRE: {  // an abandoned camp's fire: a ring of cold logs until the hunter walks up and lights it
                    const int ci = (int)pr.ph;
                    const bool lit = ci >= 0 && ci < (int)world.camps.size() && world.camps[ci].lit;
                    sprW(lit ? A.campFire[((int)(timeAcc * 11.8f) + ci * 5) % CAMP_FIRE_FRAMES] : (world.mapId == B_SNOW ? A.campColdSnow : A.campCold), pr.x, pr.y);
                } break;
                case P_CAMPTENT: sprW(pr.ph > 0.5f ? A.B.snowTent : A.B.tentOld[1], pr.x, pr.y); break;  // the regular tent standing (ph 1) or the bare one come down (ph 0): the other biomes' camps (A.B.snowTent is the regular tent, whatever its name)
                case P_BENCH: sprW(A.T.bench, pr.x, pr.y); break;
                case P_PLANTER: sprW(A.T.planter, pr.x, pr.y); break;
                case P_WELL: sprW(A.T.well, pr.x, pr.y); break;
                // ---- desert
                case P_CACTUS0: case P_CACTUS1: sprW(A.B.cactus[pr.kind - P_CACTUS0], pr.x, pr.y); break;
                case P_PALM:
                    sprW(A.B.palm, pr.x, pr.y + 1, false, 1.f, sinf(windT * 0.9f + pr.ph) * 0.9f + windDir.x * wind * (0.5f + windGust) * 1.1f, true);  // (the palm turns on its foot, leaning downwind)
                    break;
                case P_COLUMN: sprW(A.B.column, pr.x, pr.y); break;
                case P_RUINWALL: sprW(A.B.ruinwall, pr.x, pr.y); break;
                case P_ARCH: sprW(A.B.arch, pr.x, pr.y); break;
                // ---- swamp
                case P_DEADTREE0: case P_DEADTREE1:
                    {
                        const int fr = ((int)(windT * DEAD_SPRUCE_FPS + pr.ph * DEAD_SPRUCE_FRAMES / 6.2832f) % DEAD_SPRUCE_FRAMES + DEAD_SPRUCE_FRAMES) % DEAD_SPRUCE_FRAMES;
                        sprWLean(A.deadSpruce[fr], pr.x, pr.y + 1, windLean(A.deadSpruce[fr]), pr.kind == P_DEADTREE1);  // the second look is the mirror image
                    }
                    break;
                case P_LOG: sprW(A.B.log, pr.x, pr.y); break;
                case P_STILTHUT: sprW(A.B.stilthut, pr.x, pr.y); break;
                case P_WITCHTREE: sprW(A.B.witchtree, pr.x, pr.y); break;
                case P_GRAVE: sprW(A.B.grave, pr.x, pr.y); break;
                // ---- snow
                case P_SNOWPINE:
                    {
                        const int fr = ((int)(windT * SNOW_SPRUCE_FPS + pr.ph * SNOW_SPRUCE_FRAMES / 6.2832f) % SNOW_SPRUCE_FRAMES + SNOW_SPRUCE_FRAMES) % SNOW_SPRUCE_FRAMES;
                        sprWLean(A.snowSpruce[fr], pr.x, pr.y + 1, windLean(A.snowSpruce[fr]));
                        sprWG(A.snowGrass[fr % GRASS_FRAMES], pr.x, pr.y + 1, false, 1.f, windLean(A.snowGrass[fr % GRASS_FRAMES]));  // frosty grass around the roots, over the trunk
                    }
                    break;
                case P_BARETREE:
                    if (snowBirchLeafy(pr.x, pr.y)) {  // a leafy birch under snow: its sway is painted into its frames (the wind drives the pace)
                        sprW(A.B.leafyBirch[((int)(windT * LEAFY_BIRCH_FPS + pr.ph * LEAFY_BIRCH_FRAMES / 6.2832f) % LEAFY_BIRCH_FRAMES + LEAFY_BIRCH_FRAMES) % LEAFY_BIRCH_FRAMES], pr.x, pr.y + 1);
                        frostRoots(pr, false, 0.f);
                        break;
                    }
                    if (snowBirchAt(pr.x, pr.y)) {  // a birch: animated like the spruces (24 sway frames), the second look is the mirror image
                        const int fr = ((int)(windT * SNOW_BIRCH_FPS + pr.ph * SNOW_BIRCH_FRAMES / 6.2832f) % SNOW_BIRCH_FRAMES + SNOW_BIRCH_FRAMES) % SNOW_BIRCH_FRAMES;
                        sprWLean(A.snowBirch[fr], pr.x, pr.y + 1, windLean(A.snowBirch[fr]), snowBirchLook(pr.x, pr.y) == 1);
                        frostRoots(pr, false, 0.f);  // frosty tufts round the roots, drawn over the trunk
                        break;
                    }
                    {
                        const int fr = ((int)(windT * DEAD_SPRUCE_FPS + pr.ph * DEAD_SPRUCE_FRAMES / 6.2832f) % DEAD_SPRUCE_FRAMES + DEAD_SPRUCE_FRAMES) % DEAD_SPRUCE_FRAMES;
                        sprWLean(A.deadSpruce[fr], pr.x, pr.y + 1, windLean(A.deadSpruce[fr]), ((int)pr.x >> 4) & 1);  // mirrored on every other tile, so they differ
                        frostRoots(pr, true, windLean(A.snowGrassDead[fr % GRASS_FRAMES]));
                    }
                    break;
                case P_CRYSTAL: {
                    sprW(A.B.crystal, pr.x, pr.y);
                    float tw = fmodf(timeAcc * 1.1f + pr.ph, 2.6f);
                    if (tw < 0.2f) fillRect((int)pr.x - camX + 1, (int)pr.y - camY - 18, 1, 3, RGBA(255, 255, 255, 230));
                    break;
                }
                case P_ICESPIKE: sprW(A.iceSpike[propRsId[e.i] >= 0 ? spikeStage(e.i) : (int)pr.ph % ICE_SPIKE_STAGES][((int)pr.ph / ICE_SPIKE_STAGES) & 1], pr.x, pr.y); break;
                case P_ICEHUT: sprW(A.B.icehut, pr.x, pr.y); frostFoot(pr, A.B.icehut.w); break;
                case P_CAIRN: sprW(A.B.cairn, pr.x, pr.y); frostFoot(pr, A.B.cairn.w); break;
                // ---- mountain
                case P_CAVE: sprW(A.B.cave, pr.x, pr.y); break;
                case P_STANDING: {
                    sprW(A.B.standing, pr.x, pr.y);
                    float pulse = 0.5f + 0.5f * sinf(timeAcc * 1.8f + pr.ph);
                    fillRect((int)pr.x - camX - 1, (int)pr.y - camY - 21, 3, 1, RGBA(127, 214, 232, (int)(120 + 120 * pulse)));
                    break;
                }
                case P_BSIGN0: case P_BSIGN1: case P_BSIGN2: case P_BSIGN3: sprW(A.B.bsign[pr.kind - P_BSIGN0], pr.x, pr.y); break;
                // ---- the mine and the frozen dungeon
                case P_HPORTAL: sprW(A.H.portal[((int)(timeAcc * 6.f)) & 3], pr.x, pr.y); break;
                case P_MINEGATE: sprW(A.D.mineGate, pr.x, pr.y); break;
                case P_FROSTGATE: sprW(A.D.frostGate, pr.x, pr.y); break;
                case P_BEAM: break;  // drawn over the heads (below)
                case P_MINECART: sprW(A.D.cart, pr.x, pr.y); break;
                case P_TORCH: sprW(A.D.torch[((int)(timeAcc * 8.f + pr.ph * 3.f)) % 3], pr.x, pr.y); break;
                case P_CAVECRYSTAL: sprW(A.D.crystalViolet[(int)pr.ph % 3], pr.x, pr.y); break;
                case P_ICECRYSTAL: sprW(A.D.crystalIce[(int)pr.ph % 3], pr.x, pr.y); break;
                case P_STATUE: sprW(A.D.statue[(int)pr.ph % 3], pr.x, pr.y); break;
                case P_PILLAR: sprW(A.D.pillar, pr.x, pr.y); break;
                case P_STAIRS: sprW(A.D.stairs[(int)pr.ph & 1], pr.x, pr.y); break;
                case P_EXIT: sprW(A.D.exitDoor[(int)pr.ph & 1], pr.x, pr.y); break;
                case P_BOSSDOOR: sprW(A.D.bossDoor[pr.ph > 0.5f ? 1 : 0], pr.x, pr.y); break;
                case P_EQUIP: sprW(A.D.equip[(int)pr.ph & 3], pr.x, pr.y); break;
                case P_DEBRIS: sprW(A.D.debris[(int)pr.ph % 3], pr.x, pr.y); break;
                case P_SARCO: sprW(A.D.sarco, pr.x, pr.y); break;
                case P_THRONE: sprW(A.D.throne, pr.x, pr.y); break;
                case P_BRAZIER: sprW(A.D.brazier[((int)(timeAcc * 8.f + pr.ph * 2.f)) % 3], pr.x, pr.y); break;
                case P_BONES: sprW(A.D.bones[(int)pr.ph & 1], pr.x, pr.y); break;
                case P_BANNER: sprW(A.D.banner, pr.x, pr.y); break;
                case P_ICICLE: sprW(A.D.icicles, pr.x, pr.y); break;
            }
            camX = camSave;
        } else if (e.t == 9) {
            drawHsPiece(this->hs.pieces[e.i], e.j);
        } else if (e.t == 10) {
            drawHsCrop(this->hs.pieces[e.i]);
        } else if (e.t == 11) {
            drawHsAnimal(this->hs.animals[e.i]);
        } else if (e.t == 7) {
            drawMonster(mons[e.i]);
        } else if (e.t == 8) {
            drawDChest(dchests[e.i]);
        } else if (e.t == 5) {
            const Structure& st = structs[e.i];
            const V2 c = st.pos();
            if (st.kind == PL_CAMPFIRE) sprW(A.R.campfire[(int)(timeAcc * 9 + e.i) % 3], c.x, c.y);
            else sprW(A.R.chest, c.x, c.y);
        } else if (e.t == 1) {
            sprWG(A.keeper[((int)(timeAcc * 1.6f)) & 1], world.keeper.x, world.keeper.y, p.pos.x < world.keeper.x, 0.75f);
        } else if (e.t == 6) {
            const World::Npc& n = world.npcs[e.i];
            sprWG(A.T.npc[n.kind][((int)(timeAcc * 1.5f + e.i)) & 1], n.pos.x, n.pos.y, p.pos.x < n.pos.x, 0.75f);
        } else if (e.t == 12) {
            const Villager& v = villagers[e.i];
            sprWG(A.T.villager[v.look % 5][v.moving ? ((int)(v.animT * 8.f)) & 3 : 0], v.pos.x, v.pos.y, v.faceLeft, 0.75f);
        } else if (e.t == 4) {
            sprWG(A.quester[elderMoving ? ((int)(elderAnimT * 5.f)) & 1 : ((int)(timeAcc * 1.3f)) & 1], world.questGiver.x, world.questGiver.y,
                  elderMoving ? elderFaceLeft : p.pos.x < world.questGiver.x, 0.75f);
        } else if (e.t == 2) {
            const FacingView fv = facingView(p.face8);
            HAnimal* mount = p.dead ? nullptr : riddenMount();
            int st = std::max(0, std::min((int)HS_COUNT - 1, p.anim.state));
            if (mount && (st == HS_WALK || st == HS_RUN)) st = HS_IDLE;  // the mount does the walking
            const int pose = std::min(p.anim.frame(), hunterPoseCount(st) - 1);
            const Spr& s = A.hunter[fv.view][st][pose];
            const bool fl = fv.mirror;
            float rise = 0.f;
            if (mount) {  // the mount is drawn under the rider, who sits up on its back
                if (ren) ensureRichBank(ren, A, mount->species, 0, mount->tier);
                const RichAnimal* rb = A.rich[mount->species].get();
                const AnimalClips* ac = animalClips(mount->species);
                if (rb && ac) {
                    const bool mv = mountMoving;
                    const int ms = mv ? AN_WALK : AN_IDLE;
                    const int mf = mv ? (int)(mountAnimT * 7.f) % std::max(1, ac->poses[ms]) : 0;
                    const Spr& msp = rb->s[2 * mount->tier][std::max(0, std::min((int)AV_VIEWS - 1, p.face8))][ms][std::min(mf, ac->poses[ms] - 1)];
                    if (msp.ok()) {
                        sprW(msp, p.pos.x, p.pos.y, false);
                        rise = std::max(6.f, std::min(44.f, 0.6f * msp.ay * msp.sc));  // the rider sits on the back, about 60% of the way up
                    }
                }
            }
            if (p.dead) {
                // stagger, sink to the knees, then topple over
                const float fall = clampf((p.deadT - 0.32f) / 0.22f, 0.f, 1.f);
                sprW(s, p.pos.x, p.pos.y, fl, 0.9f, (fl ? -90.0 : 90.0) * fall, true);
            } else {
                sprW(s, p.pos.x, p.pos.y - rise, fl);
                if (p.invuln > 0 && (((int)(timeAcc * 18)) & 1)) {
                    Spr f = s;
                    f.t = s.flash;
                    sprW(f, p.pos.x, p.pos.y - rise, fl, 0.55f);
                }
                // a farm tool swinging at the soil: the hoe and sickle arc down like the axe, the can tips over the plot
                if (p.farmT > 0 && p.farmType >= 0 && p.farmType < HAND_SEED0 && p.tool[p.farmType] >= 0) {
                    const float f = clampf(1.f - p.farmT / FARM_SWING_T, 0.f, 1.f);
                    const bool left = p.farmAt.x < p.pos.x;
                    float ang;
                    if (p.farmType == TT_CAN) ang = 70.f * sinf(f * 3.14159f);                      // tip and back
                    else if (f < 0.8f) ang = -75.f + 125.f * (f / 0.8f) * (f / 0.8f);               // lift, then fall fast
                    else ang = 50.f - 35.f * (f - 0.8f) / 0.2f;                                     // rest on the soil, lift away
                    sprW(A.R.toolSwing[p.tool[p.farmType]], p.pos.x + (left ? -3 : 3), p.pos.y - 6, left, 1.f, left ? -ang : ang, true);
                }
                drawMelee();
                // weapon in hand while winding up
                if (p.charging && p.wpn(p.sel) > 0) {
                    float back = p.charge * 3.f;
                    V2 hp = p.pos + V2(0, -7) + p.aim * (6.f - back);
                    sprW(A.wpnWorld[p.sel], hp.x, hp.y, false, 1.f, atan2f(p.aim.y, p.aim.x) * 57.2958, true);
                }
                if (p.gathering) {
                    Animal* c = nearestCarcass(30);
                    if (c) {
                        float a = -50.f + sinf(timeAcc * 22.f) * 45.f;
                        sprW(A.wpnWorld[W_FLINT], c->pos.x + (p.pos.x < c->pos.x ? -5 : 5), c->pos.y - 7, false, 1.f, a + (p.pos.x < c->pos.x ? 0 : 180), true);
                    } else {
                        // swinging the axe / pickaxe at a tree, rock or ore: the tool arcs back and down once per swing
                        HTarget ht;
                        if (findHarvest(ht) && RES[ht.res].tool >= 0) {
                            int ti = -1;
                            if (bestTool(RES[ht.res].tool, &ti) > 0) {
                                const float f = clampf(p.gatherT / RES[ht.res].swing, 0.f, 1.f);
                                const float ease = f < 0.7f ? f / 0.7f * 0.35f : 0.35f + (f - 0.7f) / 0.3f * 0.65f;  // slow wind-up, fast strike
                                const bool left = ht.pos.x < p.pos.x;
                                float ang = -75.f + 125.f * ease;
                                if (p.impactT > 0) ang = 50.f - 35.f * (1.f - p.impactT / 0.16f);  // the tool rests on the target, then lifts as the hunter recovers
                                sprW(A.R.toolSwing[ti], p.pos.x + (left ? -3 : 3), p.pos.y - 6, left, 1.f, left ? -ang : ang, true);
                            }
                        }
                    }
                }
            }
        } else {
            Animal& a = animals[e.i];
            float spd = a.vel.len();
            const AnimalDef& D = ANIMAL[a.kind];
            float hop = (D.hop && spd > 1) ? fabsf(sinf(a.animT * 3.14159f * 2)) * 3.f : 0.f;
            if (a.st == AS_FEED && !a.dead) hop = fabsf(sinf(timeAcc * 4.f + a.animT)) * 1.2f;  // head-down bob
            hop += a.air + (a.air > 0 ? sinf(timeAcc * 3.f + a.animT) * 1.5f : 0.f);
            int fr = (a.st == AS_REST || a.st == AS_FEED) ? 0 : (spd > 1 ? ((int)a.animT) & 3 : 0);
            // species with a full animation set draw from their own bank (8 facings, six states); the rest use the walk cycle
            if (ren) ensureRichBank(ren, A, a.kind, a.variant, a.tier);  // (safety net: normally built when it spawned)
            const RichAnimal* rich = A.rich[a.kind].get();
            bool mirror = a.face < 0;
            const Spr* sp = &A.animal[a.kind][a.variant + 2 * a.tier][a.dead ? 4 : fr];
            const bool vertical = !animalClips(a.kind) && (a.face8 == F_S || a.face8 == F_N) && A.animalNS[a.kind][a.variant + 2 * a.tier][0][0].ok();
            if (vertical) {  // crawlers: front and back views
                sp = &A.animalNS[a.kind][a.variant + 2 * a.tier][a.face8 == F_N][a.dead ? 4 : fr];
                mirror = false;
            } else if (!animalClips(a.kind)) mirror = a.face8 == F_W || (a.face8 != F_E && a.face < 0);
            if (rich) {
                const AnimalClips* ac = animalClips(a.kind);
                const int st = std::max(0, std::min((int)AN_COUNT - 1, a.anim.state));
                sp = &rich->s[a.variant + 2 * a.tier][std::max(0, std::min((int)AV_VIEWS - 1, a.face8))][st][std::min(a.anim.frame(), ac->poses[st] - 1)];
                mirror = false;  // all eight directions are drawn, none is a mirror
                hop = (D.flags & BH_FLIES) ? a.air : 0.f;  // flyers keep their height; the wing beat is in the sprite
            }
            const Spr& s = *sp;
            if (a.tier == TR_LEGENDARY && !a.dead) {  // golden pulse so a legendary is easy to spot
                float pulse = 0.35f + 0.2f * sinf(timeAcc * 3.f);
                if (((int)(timeAcc * 6) + e.i) % 5 == 0)
                    fillRect((int)a.pos.x - camX + (int)(sinf(timeAcc * 5 + e.i) * 9), (int)a.pos.y - camY - (int)(a.bodyH * 2 + 4 + 5 * sinf(timeAcc * 7)), 2, 2, TIER[3].col);
            }
            if (a.tier == TR_RARE && !a.dead) {  // a rare one: a small blue glint that comes and goes above it, so it can be told from a common one from afar
                const int ph = ((int)(timeAcc * 2.5f) + e.i * 3) % 8;
                const int gx = (int)a.pos.x - camX + (e.i % 5) - 2, gy = (int)a.pos.y - camY - (int)(a.bodyH * 2 + 5);
                if (ph == 0) fillRect(gx, gy, 1, 3, TIER[2].col), fillRect(gx - 1, gy + 1, 3, 1, TIER[2].col);  // a little cross glint
                else if (ph == 1) fillRect(gx, gy + 1, 1, 1, TIER[2].col);
            }
            const bool shaded = shadeBegin(s, a.pos.x, a.pos.y, 0.75f);  // sampled on the ground, not up in the air
            sprW(s, a.pos.x, a.pos.y - hop, mirror);
            if (shaded) shadeEnd(s);
            if (a.flash > 0) {
                Spr f = s;
                f.t = s.flash;
                sprW(f, a.pos.x, a.pos.y - hop, mirror, a.flash / 0.16f * 0.85f);
            }
            if (a.st == AS_REST && !a.dead && ((int)(timeAcc * 1.5f) & 1))
                text((int)a.pos.x - camX + 4, (int)a.pos.y - camY - (int)(a.bodyH * 2 + 6), "Z", C_DIM, 1, true);
            if (!a.dead && (a.stunT > 0 || a.fearT > 0 || a.poisonT > 0)) {  // status pips above the animal: yellow stun, white fear, green poison
                int px = (int)a.pos.x - camX - 4, py = (int)a.pos.y - camY - (int)(a.bodyH * 2 + 14 + a.air);
                if (a.stunT > 0) { fillRect(px, py, 2, 2, C_GOLD); px += 3; }
                if (a.fearT > 0) { fillRect(px, py, 2, 2, C_TEXT); px += 3; }
                if (a.poisonT > 0) fillRect(px, py, 2, 2, HEX(0x6fcf4f));
            }
            if (a.hpShow > 0 && !a.dead) {
                int bx = (int)a.pos.x - camX - 9, by = (int)a.pos.y - camY - (int)(a.bodyH * 2 + D.hpBarY + a.air);
                bar(bx, by, 18, 2, (float)a.hp / a.maxhp, HEX(0xd8443f), HEX(0x3a1a18));
                if (a.tier > 0) textC((int)a.pos.x - camX, by - 7, animalName(a.kind, a.tier), TIER[a.tier].col, 1, true);
            }
        }
    }

    if (delve >= 0) {  // support beams over the corridors: the hunter walks under them
        for (auto it = std::lower_bound(world.props.begin(), world.props.end(), camY - 20.f, yLess); it != world.props.end() && it->y <= camY + LH + 90; ++it)
            if (it->kind == P_BEAM && it->x > camX - 60 && it->x < camX + LW + 60) sprW(it->ph < 0.5f ? A.D.beamArch : A.D.beamSide, it->x, it->y);
        drawDelveOverlay();
    }
    drawHsRoofs();
    drawHsOverlay();
    drawPlaceGhost();
    // projectiles
    for (auto& pr : projs) {
        float ang = (pr.angle + pr.spin * (pr.travelled / pr.speed)) * 57.2958;
        sprW(pr.ammo >= 0 ? A.ammoWorld[pr.ammo] : A.wpnWorld[pr.wpn], pr.pos.x, pr.pos.y, false, 1.f, ang, true);
        if (pr.crit) fillRect((int)pr.pos.x - camX, (int)pr.pos.y - camY, 1, 1, RGBA(255, 200, 80, 220));
    }
    // rings spreading over the water (a diving seal): a flattened circle that widens and fades
    for (const Ripple& r : ripples) {
        if (r.t <= 0.f) continue;
        const float k = r.t / r.max, rad = 6.f + 20.f * k;
        const u32 c = RGBA(70, 140, 200, (int)(235.f * (1.f - k)));  // a blue ring on the ice, fading as it spreads
        for (int i = 0; i < 32; i++) {
            const float ang = i * 6.2832f / 32.f;
            fillRect((int)(r.x + cosf(ang) * rad - camX), (int)(r.y + sinf(ang) * rad * 0.5f - camY), 1, 1, c);
        for (int i = 0; i < 32; i += 2) {  // (a lighter inner edge, so it reads as a wave)
            const float ang = i * 6.2832f / 32.f;
            fillRect((int)(r.x + cosf(ang) * (rad - 1.f) - camX), (int)(r.y + sinf(ang) * (rad - 1.f) * 0.5f - camY), 1, 1, RGBA(232, 246, 255, (int)(170.f * (1.f - k))));
        }
        }
    }
    // particles
    for (auto& q : parts) {
        float a = clampf(q.life / q.max, 0, 1);
        u32 c = withA(q.col, (int)(255 * std::min(1.f, a * 2.f)));
        fillRect((int)(q.x - camX), (int)(q.y - q.z - camY), q.size, q.size, c);
    }
    for (auto& q : smoke) {  // chimney and campfire smoke: a small puff that swells as it rises and fades out, with a darker underside
        const float k = 1.f - clampf(q.life / q.max, 0.f, 1.f);
        const int n = 3 + (int)(k * (q.size - 2.001f));
        const int a = (int)(215.f * std::min(1.f, k * 24.f) * (1.f - k * k * k));
        if (a < 6) continue;
        const int sx = (int)(q.x - camX) - n / 2, sy = (int)(q.y - q.z - camY) - n / 2;
        // grey, darker than the snow behind it (a white puff is lost on the white ground): a lit top edge, a shaded underside
        fillRect(sx + 1, sy, n - 2, n, RGBA(116, 126, 152, a));
        fillRect(sx, sy + 1, n, n - 2, RGBA(116, 126, 152, a));
        fillRect(sx + 1, sy + n - 1, n - 2, 1, RGBA(78, 88, 114, a));
        fillRect(sx + 1, sy, n - 2, 1, RGBA(170, 180, 202, a * 4 / 5));
    }
    for (auto& q : spin) {  // spindrift and pine puffs: a speck, and its faint shadow on the snow below so it shows against the white ground
        const float f = std::min(1.f, clampf(q.life / q.max, 0, 1) * 2.f);
        if (q.z > 2.f) fillRect((int)(q.x - camX), (int)(q.y - camY) + 1, q.size, 1, RGBA(112, 138, 180, (int)(95 * f)));
        fillRect((int)(q.x - camX), (int)(q.y - q.z - camY), q.size, q.size, withA(q.col, (int)(225 * f)));
    }

    // ---- lighting: darkness with pools of light, dusk and dawn colour, glows, lightning (game_light.cpp; the delves have their own darkness)
    if (delve >= 0) drawDelveDark();
    else drawLighting(night);
    if (interior < 0 && delve < 0) drawBiomeFx();
    float night2 = delve >= 0 ? 1.f : (p.nightT > 0 ? night * 0.3f : night);
    // fireflies at night / pollen by day
    // (none in the desert or the snow: it is too dry / too cold for them)
    float flyW = bw[B_HOME] + bw[B_SWAMP] + bw[B_MOUNTAIN] * 0.4f;
    for (auto& f : flies) {
        if (interior >= 0 || delve >= 0) break;
        if (night2 > 0.25f) {
            float b = (0.5f + 0.5f * sinf(timeAcc * 3.f * f.sp + f.ph)) * flyW;
            if (b < 0.02f) continue;
            sprS(A.glowCool, (int)f.x, (int)f.y, false, b * night2);
            fillRect((int)f.x, (int)f.y, 1, 1, RGBA(255, 255, 200, (int)(255 * b)));
        } else if (night2 < 0.15f && f.ph < 2.2f && flyW > 0.5f) {
            fillRect((int)f.x, (int)f.y, 1, 1, RGBA(255, 255, 255, 120));
        }
    }
    drawCompanion();
    drawParked();
    drawFishing();
    drawSkyFx(night2);  // aurora, shimmer, wisps, rain
    sprS(A.vignette, 0, 0);

    // floating texts
    for (auto& f : ftexts) {
        float k = f.t / f.life;
        float a = k > 0.7f ? 1.f - (k - 0.7f) / 0.3f : 1.f;
        int w = textW(f.s);
        textOutlined((int)f.p.x - camX - w / 2, (int)f.p.y - camY, f.s, f.col, a);
    }
}
