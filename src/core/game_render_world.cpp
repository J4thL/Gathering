// game_render_world.cpp - drawing the world: ground, props, animals, the hunter, weather and light.
#include <cstdio>
#include "core/game.hpp"
#include "core/logic_util.hpp"

// ================================================================= world
void Game::renderWorld(bool) {
    int tx0 = std::max(0, camX / TS), ty0 = std::max(0, camY / TS);
    int tx1 = std::min(MW - 1, (camX + LW) / TS), ty1 = std::min(MH - 1, (camY + LH) / TS);
    int wf = (int)(timeAcc * 2.4f);
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++) {
            int sx = tx * TS - camX, sy = ty * TS - camY;
            uint32_t h = hash2(tx - COX, ty - COY, 5);  // home-relative, so the original area keeps its exact tile variants
            switch (world.tile[ty][tx]) {
                case T_GRASS: sprS(A.grass[h % 3], sx, sy); break;
                case T_DARK: sprS(A.grass[3], sx, sy); break;
                case T_DIRT: sprS(A.dirt[h % 2], sx, sy); break;
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
                    sprS(A.B.cliff[h % 3], sx, sy);
                    if (!world.tileIs(tx, ty - 1, T_CLIFF)) fillRect(sx, sy, TS, 2, HEX(0x9b9eb2));
                    if (!world.tileIs(tx, ty + 1, T_CLIFF)) fillRect(sx, sy + TS - 3, TS, 3, HEX(0x2c2d3a));
                    break;
                }
                case T_WATER: {
                    sprS(A.water[(wf + (hash2((tx - COX) / 2, (ty - COY) / 2, 9) & 3)) & 3], sx, sy);
                    u32 foam = RGBA(235, 248, 255, 150);
                    if (!world.water(tx, ty - 1)) fillRect(sx, sy, TS, 2, foam);
                    if (!world.water(tx, ty + 1)) fillRect(sx, sy + TS - 2, TS, 2, foam);
                    if (!world.water(tx - 1, ty)) fillRect(sx, sy, 2, TS, foam);
                    if (!world.water(tx + 1, ty)) fillRect(sx + TS - 2, sy, 2, TS, foam);
                    break;
                }
            }
        }
    // blood stains
    if (inHomestead()) drawHomesteadGround();
    for (auto& d : decals) {
        float a = std::min(1.f, (d.life - d.t) / 8.f) * 0.9f;
        sprW(A.blood[d.size], d.p.x, d.p.y, false, a);
    }
    // thrown weapons lying on the ground
    for (auto& it : items) {
        sprW(A.shadow[0], it.pos.x, it.pos.y, false, 0.6f);
        if (it.item >= 0) sprW(itemIcon(it.item), it.pos.x, it.pos.y - 5, false, 1.f);  // a stack the hunter dropped
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
        const Spr& s = A.animal[a.kind][a.variant + 2 * a.tier][4];
        sprW(A.shadow[1], a.pos.x, a.pos.y + 2, false, 0.7f * a.fade);
        sprW(s, a.pos.x, a.pos.y - 6, a.face < 0, a.fade);
        if (a.flash > 0) {
            Spr f = s;
            f.t = s.flash;
            sprW(f, a.pos.x, a.pos.y - 6, a.face < 0, a.flash / 0.16f * 0.8f);
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
        switch (d.kind) {
            case D_FLOWER0: case D_FLOWER1: case D_FLOWER2: s = &A.flower[d.kind]; break;
            case D_TUFT: s = &A.tuft; break;
            case D_MUSHROOM: s = &A.mushroom; sway = false; break;
            case D_PEBBLES: s = &A.pebbles; sway = false; break;
            case D_BUSH0: case D_BUSH1: s = &A.bush[d.kind - D_BUSH0]; sway = false; break;
            case D_DRYBUSH: s = &A.B.drybush; swayAmt = 3.f; break;
            case D_SKULL: s = &A.B.skull; sway = false; break;
            case D_SANDTUFT: s = &A.B.sandtuft; break;
            case D_REEDS0: case D_REEDS1: s = &A.B.reeds[d.kind - D_REEDS0]; swayAmt = 4.f; break;
            case D_LILY: s = &A.B.lily; sway = false; break;
            case D_BUBBLES: s = &A.B.bubbles; sway = false; alpha = fmaxf(0.f, sinf(timeAcc * 1.3f + d.ph * 3.f)) * 0.9f + 0.1f; break;
            case D_DRIFT: s = &A.B.drift; sway = false; break;
            case D_FROSTTUFT: s = &A.B.frosttuft; swayAmt = 3.f; break;
            case D_ALPINE: s = &A.B.alpine; break;
            default: s = &A.B.gravel; sway = false; break;
        }
        float ang = sway ? sinf(timeAcc * 1.9f + d.ph + d.x * 0.02f) * swayAmt : 0.f;
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
        sprW(A.shadow[0], n.x + shk, n.y, false, 0.55f * grow);
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
    for (auto& e : dl) {
        if (e.t == 0) {
            const Prop& pr = world.props[e.i];
            const int camSave = camX;
            const int rs = propRsId[e.i];
            if (rs >= 0) {
                const HState& hs = propSt[e.i];
                if (hs.gone || hs.fade > 0) {  // felled tree / broken rock, then a sprout while it grows back
                    const bool tree = RES[rs].cls == RCL_TREE;
                    sprW(A.shadow[0], pr.x, pr.y, false, 0.5f);
                    sprW(tree ? A.R.stump : A.R.rubble, pr.x, pr.y);
                    if (!hs.gone && tree) sprW(A.R.sprout, pr.x, pr.y - 2, false, 0.4f + 0.6f * (1.f - hs.fade / REGROW_FADE));
                    continue;
                }
                if (hs.shake > 0) camX -= (int)(sinf(timeAcc * 70.f) * hs.shake * 9.f);  // the tree shudders / the rock rattles when struck
            }
            switch (pr.kind) {
                case P_OAK0: case P_OAK1:
                    sprW(A.shadow[2], pr.x + 4, pr.y - 1, false, 0.8f);
                    sprW(A.oak[pr.kind == P_OAK1 ? 1 : 0], pr.x, pr.y + 1, false, 1.f, sinf(timeAcc * 0.8f + pr.ph) * 0.6f, true);
                    break;
                case P_PINE:
                    sprW(A.shadow[1], pr.x + 3, pr.y - 1, false, 0.8f);
                    sprW(A.pine, pr.x, pr.y + 1, false, 1.f, sinf(timeAcc * 0.7f + pr.ph) * 0.7f, true);
                    break;
                case P_ROCK0: sprW(A.shadow[1], pr.x, pr.y, false, 0.7f); sprW(A.rock[0], pr.x, pr.y); break;
                case P_ROCK1: sprW(A.shadow[1], pr.x, pr.y, false, 0.7f); sprW(A.rock[1], pr.x, pr.y); break;
                case P_HUT: sprW(A.hut, pr.x, pr.y); break;
                case P_FIRE: sprW(A.fire[(int)(timeAcc * 9) % 3], pr.x, pr.y); break;
                case P_CRATE: sprW(A.shadow[0], pr.x, pr.y, false, 0.7f); sprW(A.crate, pr.x, pr.y); break;
                case P_BARREL: sprW(A.shadow[0], pr.x, pr.y, false, 0.7f); sprW(A.barrel, pr.x, pr.y); break;
                case P_SIGN: sprW(A.sign, pr.x, pr.y); break;
                // ---- the town
                case P_BLD0: case P_BLD1: case P_BLD2: case P_BLD3: case P_BLD4: case P_BLD5:
                    sprW(A.shadow[2], pr.x + 6, pr.y - 1, false, 0.55f);
                    sprW(A.T.bld[pr.kind - P_BLD0], pr.x, pr.y);
                    break;
                case P_FURN: sprW(A.shadow[0], pr.x, pr.y, false, 0.5f); sprW(A.T.furn[std::max(0, std::min(FU_COUNT - 1, (int)pr.ph))], pr.x, pr.y); break;
                case P_LAMP: sprW(A.T.lamp, pr.x, pr.y); break;
                case P_FENCE: sprW(A.T.fence, pr.x, pr.y); break;
                case P_BENCH: sprW(A.shadow[0], pr.x, pr.y, false, 0.6f); sprW(A.T.bench, pr.x, pr.y); break;
                case P_PLANTER: sprW(A.shadow[0], pr.x, pr.y, false, 0.6f); sprW(A.T.planter, pr.x, pr.y); break;
                case P_WELL: sprW(A.shadow[1], pr.x, pr.y, false, 0.6f); sprW(A.T.well, pr.x, pr.y); break;
                // ---- desert
                case P_CACTUS0: case P_CACTUS1: sprW(A.shadow[0], pr.x, pr.y, false, 0.7f); sprW(A.B.cactus[pr.kind - P_CACTUS0], pr.x, pr.y); break;
                case P_PALM:
                    sprW(A.shadow[2], pr.x + 5, pr.y - 1, false, 0.8f);
                    sprW(A.B.palm, pr.x, pr.y + 1, false, 1.f, sinf(timeAcc * 0.9f + pr.ph) * 0.9f, true);
                    break;
                case P_SANDROCK0: case P_SANDROCK1: sprW(A.shadow[1], pr.x, pr.y, false, 0.7f); sprW(A.B.sandrock[pr.kind - P_SANDROCK0], pr.x, pr.y); break;
                case P_COLUMN: sprW(A.shadow[0], pr.x + 3, pr.y, false, 0.7f); sprW(A.B.column, pr.x, pr.y); break;
                case P_RUINWALL: sprW(A.shadow[2], pr.x, pr.y, false, 0.6f); sprW(A.B.ruinwall, pr.x, pr.y); break;
                case P_ARCH: sprW(A.shadow[2], pr.x, pr.y, false, 0.6f); sprW(A.B.arch, pr.x, pr.y); break;
                // ---- swamp
                case P_DEADTREE0: case P_DEADTREE1:
                    sprW(A.shadow[1], pr.x + 3, pr.y - 1, false, 0.7f);
                    sprW(A.B.deadtree[pr.kind - P_DEADTREE0], pr.x, pr.y + 1, false, 1.f, sinf(timeAcc * 0.6f + pr.ph) * 0.5f, true);
                    break;
                case P_LOG: sprW(A.shadow[1], pr.x, pr.y, false, 0.7f); sprW(A.B.log, pr.x, pr.y); break;
                case P_STILTHUT: sprW(A.B.stilthut, pr.x, pr.y); break;
                case P_WITCHTREE: sprW(A.shadow[2], pr.x + 4, pr.y - 1, false, 0.8f); sprW(A.B.witchtree, pr.x, pr.y); break;
                case P_GRAVE: sprW(A.shadow[0], pr.x, pr.y, false, 0.7f); sprW(A.B.grave, pr.x, pr.y); break;
                // ---- snow
                case P_SNOWPINE:
                    sprW(A.shadow[1], pr.x + 3, pr.y - 1, false, 0.6f);
                    sprW(A.B.snowpine, pr.x, pr.y + 1, false, 1.f, sinf(timeAcc * 0.7f + pr.ph) * 0.7f, true);
                    break;
                case P_BARETREE:
                    sprW(A.shadow[1], pr.x + 3, pr.y - 1, false, 0.6f);
                    sprW(A.B.baretree, pr.x, pr.y + 1, false, 1.f, sinf(timeAcc * 0.8f + pr.ph) * 0.6f, true);
                    break;
                case P_SNOWROCK: {
                    int v = ((int)pr.x >> 4) & 1;
                    sprW(A.shadow[1], pr.x, pr.y, false, 0.6f);
                    sprW(A.B.snowrock[v], pr.x, pr.y);
                    break;
                }
                case P_CRYSTAL: {
                    sprW(A.shadow[0], pr.x, pr.y, false, 0.6f);
                    sprW(A.B.crystal, pr.x, pr.y);
                    float tw = fmodf(timeAcc * 1.1f + pr.ph, 2.6f);
                    if (tw < 0.2f) fillRect((int)pr.x - camX + 1, (int)pr.y - camY - 18, 1, 3, RGBA(255, 255, 255, 230));
                    break;
                }
                case P_ICEHUT: sprW(A.B.icehut, pr.x, pr.y); break;
                case P_CAIRN: sprW(A.shadow[0], pr.x, pr.y, false, 0.7f); sprW(A.B.cairn, pr.x, pr.y); break;
                // ---- mountain
                case P_CAVE: sprW(A.B.cave, pr.x, pr.y); break;
                case P_BOULDER: sprW(A.shadow[1], pr.x, pr.y, false, 0.7f); sprW(A.B.boulder, pr.x, pr.y); break;
                case P_STANDING: {
                    sprW(A.shadow[0], pr.x + 2, pr.y, false, 0.7f);
                    sprW(A.B.standing, pr.x, pr.y);
                    float pulse = 0.5f + 0.5f * sinf(timeAcc * 1.8f + pr.ph);
                    fillRect((int)pr.x - camX - 1, (int)pr.y - camY - 21, 3, 1, RGBA(127, 214, 232, (int)(120 + 120 * pulse)));
                    break;
                }
                case P_BSIGN0: case P_BSIGN1: case P_BSIGN2: case P_BSIGN3: sprW(A.shadow[0], pr.x, pr.y, false, 0.7f); sprW(A.B.bsign[pr.kind - P_BSIGN0], pr.x, pr.y); break;
                // ---- the mine and the frozen dungeon
                case P_HPORTAL: sprW(A.shadow[2], pr.x, pr.y, false, 0.5f); sprW(A.H.portal[((int)(timeAcc * 6.f)) & 3], pr.x, pr.y); break;
                case P_HMARKET: sprW(A.shadow[2], pr.x + 4, pr.y, false, 0.5f); sprW(A.H.market, pr.x, pr.y); break;
                case P_MINEGATE: sprW(A.D.mineGate, pr.x, pr.y); break;
                case P_FROSTGATE: sprW(A.D.frostGate, pr.x, pr.y); break;
                case P_BEAM: break;  // drawn over the heads (below)
                case P_MINECART: sprW(A.shadow[1], pr.x, pr.y, false, 0.6f); sprW(A.D.cart, pr.x, pr.y); break;
                case P_TORCH: sprW(A.D.torch[((int)(timeAcc * 8.f + pr.ph * 3.f)) % 3], pr.x, pr.y); break;
                case P_CAVECRYSTAL: sprW(A.shadow[0], pr.x, pr.y, false, 0.6f); sprW(A.D.crystalViolet[(int)pr.ph % 3], pr.x, pr.y); break;
                case P_ICECRYSTAL: sprW(A.shadow[0], pr.x, pr.y, false, 0.6f); sprW(A.D.crystalIce[(int)pr.ph % 3], pr.x, pr.y); break;
                case P_STATUE: sprW(A.shadow[0], pr.x + 2, pr.y, false, 0.7f); sprW(A.D.statue[(int)pr.ph % 3], pr.x, pr.y); break;
                case P_PILLAR: sprW(A.shadow[0], pr.x + 3, pr.y, false, 0.7f); sprW(A.D.pillar, pr.x, pr.y); break;
                case P_STAIRS: sprW(A.D.stairs[(int)pr.ph & 1], pr.x, pr.y); break;
                case P_EXIT: sprW(A.D.exitDoor[(int)pr.ph & 1], pr.x, pr.y); break;
                case P_BOSSDOOR: sprW(A.D.bossDoor[pr.ph > 0.5f ? 1 : 0], pr.x, pr.y); break;
                case P_EQUIP: sprW(A.shadow[0], pr.x, pr.y, false, 0.6f); sprW(A.D.equip[(int)pr.ph & 3], pr.x, pr.y); break;
                case P_DEBRIS: sprW(A.D.debris[(int)pr.ph % 3], pr.x, pr.y); break;
                case P_SARCO: sprW(A.shadow[2], pr.x, pr.y, false, 0.6f); sprW(A.D.sarco, pr.x, pr.y); break;
                case P_THRONE: sprW(A.shadow[2], pr.x, pr.y, false, 0.6f); sprW(A.D.throne, pr.x, pr.y); break;
                case P_BRAZIER: sprW(A.shadow[0], pr.x, pr.y, false, 0.6f); sprW(A.D.brazier[((int)(timeAcc * 8.f + pr.ph * 2.f)) % 3], pr.x, pr.y); break;
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
            sprW(A.shadow[0], c.x, c.y, false, 0.7f);
            if (st.kind == PL_CAMPFIRE) sprW(A.R.campfire[(int)(timeAcc * 9 + e.i) % 3], c.x, c.y);
            else sprW(A.R.chest, c.x, c.y);
        } else if (e.t == 1) {
            sprW(A.shadow[0], world.keeper.x, world.keeper.y, false, 0.8f);
            sprW(A.keeper[((int)(timeAcc * 1.6f)) & 1], world.keeper.x, world.keeper.y, p.pos.x < world.keeper.x);
        } else if (e.t == 6) {
            const World::Npc& n = world.npcs[e.i];
            sprW(A.shadow[0], n.pos.x, n.pos.y, false, 0.8f);
            sprW(A.T.npc[n.kind][((int)(timeAcc * 1.5f + e.i)) & 1], n.pos.x, n.pos.y, p.pos.x < n.pos.x);
        } else if (e.t == 12) {
            const Villager& v = villagers[e.i];
            sprW(A.shadow[0], v.pos.x, v.pos.y, false, 0.8f);
            sprW(A.T.villager[v.look % 5][v.moving ? ((int)(v.animT * 8.f)) & 3 : 0], v.pos.x, v.pos.y, v.faceLeft);
        } else if (e.t == 4) {
            sprW(A.shadow[0], world.questGiver.x, world.questGiver.y, false, 0.8f);
            sprW(A.quester[elderMoving ? ((int)(elderAnimT * 5.f)) & 1 : ((int)(timeAcc * 1.3f)) & 1], world.questGiver.x, world.questGiver.y,
                 elderMoving ? elderFaceLeft : p.pos.x < world.questGiver.x);
        } else if (e.t == 2) {
            const FacingView fv = facingView(p.face8);
            const int st = std::max(0, std::min((int)HS_COUNT - 1, p.anim.state));
            const int pose = std::min(p.anim.frame(), hunterPoseCount(st) - 1);
            const Spr& s = A.hunter[fv.view][st][pose];
            const bool fl = fv.mirror;
            // the light comes from the upper left: one shadow shape for every pose, falling to the lower right
            if (p.dead) sprW(A.shadow[1], p.pos.x, p.pos.y, false, 0.6f);
            else sprW(A.shadowCast, p.pos.x - 1, p.pos.y, false, st == HS_RUN ? 0.85f : 1.f);
            if (p.dead) {
                // stagger, sink to the knees, then topple over
                const float fall = clampf((p.deadT - 0.32f) / 0.22f, 0.f, 1.f);
                sprW(s, p.pos.x, p.pos.y, fl, 0.9f, (fl ? -90.0 : 90.0) * fall, true);
            } else {
                sprW(s, p.pos.x, p.pos.y, fl);
                if (p.invuln > 0 && (((int)(timeAcc * 18)) & 1)) {
                    Spr f = s;
                    f.t = s.flash;
                    sprW(f, p.pos.x, p.pos.y, fl, 0.55f);
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
                sprW(A.shadow[2], a.pos.x, a.pos.y - a.bodyH * 0.6f, false, pulse);
                if (((int)(timeAcc * 6) + e.i) % 5 == 0)
                    fillRect((int)a.pos.x - camX + (int)(sinf(timeAcc * 5 + e.i) * 9), (int)a.pos.y - camY - (int)(a.bodyH * 2 + 4 + 5 * sinf(timeAcc * 7)), 2, 2, TIER[3].col);
            }
            sprW(A.shadow[std::min(2, D.shadow)], a.pos.x, a.pos.y, false, 0.9f * (1.f - clampf(a.air / 30.f, 0.f, 0.4f)));  // a high flyer's shadow is fainter
            sprW(s, a.pos.x, a.pos.y - hop, mirror);
            if (a.flash > 0) {
                Spr f = s;
                f.t = s.flash;
                sprW(f, a.pos.x, a.pos.y - hop, mirror, a.flash / 0.16f * 0.85f);
            }
            if (a.st == AS_REST && !a.dead && ((int)(timeAcc * 1.5f) & 1))
                text((int)a.pos.x - camX + 4, (int)a.pos.y - camY - (int)(a.bodyH * 2 + 6), "Z", C_DIM, 1, true);
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
        sprW(A.shadow[0], pr.pos.x, pr.pos.y + 9, false, 0.45f);
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

    // ---- lighting: day/night tint, warm glows, vignette
    if (delve >= 0) {
        drawDelveDark();
    } else {
    float night2 = p.nightT > 0 ? night * 0.3f : night;  // a nightsight tonic turns night into dusk
    if (night2 > 0.01f) fillRect(0, 0, LW, LH, RGBA(14, 22, 70, (int)(night2 * 128)));
    float sun = sinf(tod() * 6.2832f);
    float warm = interior >= 0 ? 0.f : clampf(1.f - fabsf(sun) * 3.2f, 0, 1);
    if (warm > 0.01f) fillRect(0, 0, LW, LH, RGBA(255, 120, 50, (int)(warm * 46)));
    {
        float fl = 0.85f + 0.15f * sinf(timeAcc * 11.f) + 0.05f * sinf(timeAcc * 27.f);
        float strength = 0.28f + 0.72f * night2;
        SDL_Texture* g = A.glowWarm.t;
        SDL_SetTextureAlphaMod(g, (Uint8)(255 * clampf(strength * fl, 0, 1)));
        int sz = (int)(150 * fl);
        SDL_Rect d{(int)world.fire.x - camX - sz / 2, (int)world.fire.y - 12 - camY - sz / 2, sz, sz};
        SDL_RenderCopy(ren, g, nullptr, &d);
        if (night2 > 0.05f) {
            SDL_SetTextureAlphaMod(g, (Uint8)(190 * night2));
            for (int wx : {-19, 13}) {
                SDL_Rect w{(int)world.hut.x + wx - camX - 22, (int)world.hut.y - 20 - camY - 22, 44, 44};
                SDL_RenderCopy(ren, g, nullptr, &w);
            }
            SDL_Rect pl{(int)p.pos.x - camX - 34, (int)p.pos.y - 8 - camY - 34, 68, 68};
            SDL_SetTextureAlphaMod(g, (Uint8)(110 * night2));
            if (state != ST_TITLE) SDL_RenderCopy(ren, g, nullptr, &pl);
        }
        for (auto& lt : world.lanterns) {
            if (lt.x < camX - 60 || lt.x > camX + LW + 60 || lt.y < camY - 60 || lt.y > camY + LH + 60) continue;
            float lf = 0.85f + 0.15f * sinf(timeAcc * 9.f + lt.x);
            SDL_SetTextureAlphaMod(g, (Uint8)(clampf(lt.r * lf * (0.3f + 0.7f * night2), 0, 1) * 255));
            int sz = (int)(78 * lf);
            SDL_Rect d{(int)lt.x - camX - sz / 2, (int)lt.y - camY - sz / 2, sz, sz};
            SDL_RenderCopy(ren, g, nullptr, &d);
        }
        for (auto& st : structs) {  // placed campfires light the night
            if (st.kind != PL_CAMPFIRE) continue;
            const V2 c = st.pos();
            if (c.x < camX - 70 || c.x > camX + LW + 70 || c.y < camY - 70 || c.y > camY + LH + 70) continue;
            float lf = 0.85f + 0.15f * sinf(timeAcc * 10.f + c.x) + 0.05f * sinf(timeAcc * 23.f);
            SDL_SetTextureAlphaMod(g, (Uint8)(clampf((0.4f + 0.6f * night2) * lf, 0, 1) * 255));
            int sz = (int)(120 * lf);
            SDL_Rect d{(int)c.x - camX - sz / 2, (int)c.y - 8 - camY - sz / 2, sz, sz};
            SDL_RenderCopy(ren, g, nullptr, &d);
        }
        drawHsLights(night2);
        SDL_SetTextureAlphaMod(g, 255);
    }
    }  // (not a delve)
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
    sprS(A.vignette, 0, 0);

    // floating texts
    for (auto& f : ftexts) {
        float k = f.t / f.life;
        float a = k > 0.7f ? 1.f - (k - 0.7f) / 0.3f : 1.f;
        int w = textW(f.s);
        textOutlined((int)f.p.x - camX - w / 2, (int)f.p.y - camY, f.s, f.col, a);
    }
}
