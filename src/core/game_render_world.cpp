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
void Game::renderWorld(bool) {
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
                case T_SNOW: sprS(A.B.snow[h % 3], sx, sy); break;
                case T_SNOWP: sprS(A.B.snowp[h % 2], sx, sy); break;
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
                        sprS(world.tileIs(tx, ty - 1, T_CLIFF) ? A.B.snowCliff[h % 3] : A.B.snowCliffCap[h % 3], sx, sy);
                        if (!world.tileIs(tx, ty + 1, T_CLIFF)) fillRect(sx, sy + TS - 3, TS, 3, HEX(0x2e2a34));
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
        if (it.item >= 0) { sprW(A.shadow[0], it.pos.x, it.pos.y, false, 0.6f); icon(itemIcon(it.item), (int)it.pos.x - camX, (int)it.pos.y - camY - 4, DROP_ICON); }  // a stack the hunter dropped: a small icon, not a whole tile
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
    for (auto it = std::lower_bound(world.decor.begin(), world.decor.end(), camY - 10.f, yLess); it != world.decor.end() && it->y <= camY + LH + 30; ++it) {
        const Decor& d = *it;
        if (d.x < camX - 20 || d.x > camX + LW + 20) continue;
        const Spr* s = nullptr;
        bool sway = true;
        float swayAmt = 6.f, alpha = 1.f;
        bool squash = false;  // round bushes: no idle sway, a springy squash when brushed
        switch (d.kind) {
            case D_FLOWER0: case D_FLOWER1: case D_FLOWER2: s = &A.flower[d.kind]; break;
            case D_TUFT: s = &A.tuft; break;
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
                break;
            }
            case D_DRYBUSH: s = &A.B.drybush; swayAmt = 3.f; break;
            case D_SKULL: s = &A.B.skull; sway = false; break;
            case D_DROCK0: case D_DROCK1: case D_DROCK2: case D_DROCK3: s = &A.desertDecor[d.kind - D_DROCK0]; sway = false; break;
            case D_SANDTUFT: s = &A.B.sandtuft; swayAmt = 1.5f; break;  // the agave: stiff leaves, so only a hint of sway
            case D_REEDS0: case D_REEDS1: s = &A.B.reeds[d.kind - D_REEDS0]; swayAmt = 4.f; break;
            case D_LILY: s = &A.B.lily; sway = false; break;
            case D_BUBBLES: s = &A.B.bubbles; sway = false; alpha = fmaxf(0.f, sinf(timeAcc * 1.3f + d.ph * 3.f)) * 0.9f + 0.1f; break;
            case D_DRIFT: s = &A.B.drift; sway = false; break;
            case D_FROSTTUFT: s = &A.B.frosttuft; swayAmt = 3.f; break;
            case D_ALPINE: s = &A.B.alpine; break;
            case D_STUMP: s = &A.R.stump; sway = false; break;
            case D_VIOLET: case D_POPPY: s = &A.flowerX[d.kind - D_VIOLET]; break;
            default: s = &A.B.gravel; sway = false; break;
        }
        float ang = sway ? sinf(windT * 1.9f + d.ph + d.x * 0.02f) * swayAmt * (0.8f + wind) + grassBend((size_t)(it - world.decor.begin())) : 0.f;  // plus the lean from whoever just walked through
        if (squash) {  // the bend is the lean a walker's kick would give a tuft: a bush turns it into a short squat-and-spring and a slight tilt
            const float bend = grassBend((size_t)(it - world.decor.begin())), k = std::min(1.f, fabsf(bend) / 30.f);
            sprWSquash(*s, d.x, d.y, 1.f + 0.10f * k, 1.f - 0.12f * k, bend * 0.12f, alpha);
        } else
            sprW(*s, d.x, d.y, false, alpha, ang, true);
    }
    // resource nodes: a soft shadow, the plant or ore, and a slow glint so they read as "pick me"
    for (auto it = std::lower_bound(world.nodes.begin(), world.nodes.end(), camY - 10.f, yLess); it != world.nodes.end() && it->y <= camY + LH + 30; ++it) {
        const ResNode& n = *it;
        if (n.x < camX - 20 || n.x > camX + LW + 20) continue;
        const HState& hs = nodeSt[it - world.nodes.begin()];
        if (hs.gone) {  // finished: a stub of the plant, or the broken-up rock, until it grows back
            sprW(A.R.stub.t && RES[nodeRes(n.kind)].cls == RCL_PLANT ? A.R.stub : A.R.rubble, n.x, n.y);
            continue;
        }
        const float grow = hs.fade > 0 ? 1.f - hs.fade / REGROW_FADE : 1.f;  // regrowing: fades back in
        const float shk = hs.shake > 0 ? sinf(timeAcc * 70.f) * hs.shake * 9.f : 0.f;
        bool plant = n.kind != RN_ORE && n.kind != RN_CACTUS && n.kind != RN_COPPER && n.kind != RN_GOLD && (n.kind < RN_COAL || n.kind >= RN_BERRY);
        sprW(n.kind >= RN_BERRY ? A.H.node[n.kind - RN_BERRY] : n.kind >= RN_COAL ? A.D.node[n.kind - RN_COAL] : A.B.node[n.kind], n.x + shk, n.y, false, std::max(0.25f, grow), plant ? sinf(timeAcc * 1.6f + n.ph) * 3.f : 0.f, true);
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
    for (auto it = std::lower_bound(world.props.begin(), world.props.end(), camY - 20.f, yLess); it != world.props.end() && it->y <= camY + LH + 90; ++it) {
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
        if (gt != T_GRASS && gt != T_DARK && gt != T_MOSS) return;  // not on sand, snow, stone, bare forest floors or the interior floors
        const int kind = rockClass(pr.kind);
        const int look = rockLookOf(pr);
        const int fr = ((int)(windT * 5.f + pr.ph * ROCK_GRASS_FRAMES / 6.2832f) % ROCK_GRASS_FRAMES + ROCK_GRASS_FRAMES) % ROCK_GRASS_FRAMES;
        sprWG(A.rockGrass[kind][look][fr], pr.x, pr.y + 2);
    };
    for (auto& e : dl) {
        if (e.t == 0) {
            const Prop& pr = world.props[e.i];
            const int camSave = camX;
            const int rs = propRsId[e.i];
            if (rs >= 0) {
                const HState& hs = propSt[e.i];
                if (hs.gone || hs.fade > 0) {  // felled tree / broken rock, then a sprout while it grows back
                    const bool tree = RES[rs].cls == RCL_TREE;
                    sprW(tree ? A.R.stump : (isRockKind(pr.kind) ? A.rubbleT[rockTheme()] : A.R.rubble), pr.x, pr.y);
                    if (!tree && rockClass(pr.kind) >= 1) pebblesAround(pr);  // the pebbles stay where the rock was
                    if (!tree && isRockKind(pr.kind)) grassAround(pr);
                    if (tree && pr.kind == P_SNOWPINE)  // the frosty grass stays around the stump too
                        sprWG(A.snowGrass[((int)(windT * SNOW_SPRUCE_FPS + pr.ph * GRASS_FRAMES / 6.2832f) % GRASS_FRAMES + GRASS_FRAMES) % GRASS_FRAMES], pr.x, pr.y + 1);
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
                        sprW(A.oak[ov][fr], pr.x, pr.y + 1);
                        if (!bareFloor(pr)) sprWG(A.treeGrass[OAK_VAR[ov].grass][fr], pr.x, pr.y + 1);  // the grass around the roots, drawn over the trunk
                    }
                    break;
                case P_PINE:
                    {
                        const int fr = ((int)(windT * PINE_FPS + pr.ph * PINE_FRAMES / 6.2832f) % PINE_FRAMES + PINE_FRAMES) % PINE_FRAMES;
                        sprW(A.pine[fr], pr.x, pr.y + 1);
                        if (!bareFloor(pr)) sprWG(A.treeGrass[GRASS_SPRUCE][fr], pr.x, pr.y + 1);  // the grass around the roots, drawn over the trunk
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
                case P_BENCH: sprW(A.T.bench, pr.x, pr.y); break;
                case P_PLANTER: sprW(A.T.planter, pr.x, pr.y); break;
                case P_WELL: sprW(A.T.well, pr.x, pr.y); break;
                // ---- desert
                case P_CACTUS0: case P_CACTUS1: sprW(A.B.cactus[pr.kind - P_CACTUS0], pr.x, pr.y); break;
                case P_PALM:
                    sprW(A.B.palm, pr.x, pr.y + 1, false, 1.f, sinf(windT * 0.9f + pr.ph) * 0.9f, true);
                    break;
                case P_COLUMN: sprW(A.B.column, pr.x, pr.y); break;
                case P_RUINWALL: sprW(A.B.ruinwall, pr.x, pr.y); break;
                case P_ARCH: sprW(A.B.arch, pr.x, pr.y); break;
                // ---- swamp
                case P_DEADTREE0: case P_DEADTREE1:
                    {
                        const int fr = ((int)(windT * DEAD_SPRUCE_FPS + pr.ph * DEAD_SPRUCE_FRAMES / 6.2832f) % DEAD_SPRUCE_FRAMES + DEAD_SPRUCE_FRAMES) % DEAD_SPRUCE_FRAMES;
                        sprW(A.deadSpruce[fr], pr.x, pr.y + 1, pr.kind == P_DEADTREE1);  // the second look is the mirror image
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
                        sprW(A.snowSpruce[fr], pr.x, pr.y + 1);
                        sprWG(A.snowGrass[fr % GRASS_FRAMES], pr.x, pr.y + 1);  // frosty grass around the roots, over the trunk
                    }
                    break;
                case P_BARETREE:
                    {
                        const int fr = ((int)(windT * DEAD_SPRUCE_FPS + pr.ph * DEAD_SPRUCE_FRAMES / 6.2832f) % DEAD_SPRUCE_FRAMES + DEAD_SPRUCE_FRAMES) % DEAD_SPRUCE_FRAMES;
                        sprW(A.deadSpruce[fr], pr.x, pr.y + 1, ((int)pr.x >> 4) & 1);  // mirrored on every other tile, so they differ
                    }
                    break;
                case P_CRYSTAL: {
                    sprW(A.B.crystal, pr.x, pr.y);
                    float tw = fmodf(timeAcc * 1.1f + pr.ph, 2.6f);
                    if (tw < 0.2f) fillRect((int)pr.x - camX + 1, (int)pr.y - camY - 18, 1, 3, RGBA(255, 255, 255, 230));
                    break;
                }
                case P_ICEHUT: sprW(A.B.icehut, pr.x, pr.y); break;
                case P_CAIRN: sprW(A.B.cairn, pr.x, pr.y); break;
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
                case P_HMARKET: sprW(A.H.market, pr.x, pr.y); break;
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
    // projectiles
    for (auto& pr : projs) {
        float ang = (pr.angle + pr.spin * (pr.travelled / pr.speed)) * 57.2958;
        sprW(pr.ammo >= 0 ? A.ammoWorld[pr.ammo] : A.wpnWorld[pr.wpn], pr.pos.x, pr.pos.y, false, 1.f, ang, true);
        if (pr.crit) fillRect((int)pr.pos.x - camX, (int)pr.pos.y - camY, 1, 1, RGBA(255, 200, 80, 220));
    }
    // particles
    for (auto& q : parts) {
        float a = clampf(q.life / q.max, 0, 1);
        u32 c = withA(q.col, (int)(255 * std::min(1.f, a * 2.f)));
        fillRect((int)(q.x - camX), (int)(q.y - q.z - camY), q.size, q.size, c);
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
