// game_render.cpp - world rendering, HUD, shop UI, menus.
#include <cstdio>
#include "game.hpp"

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

// ================================================================= 2D primitives
void Game::fillRect(int x, int y, int w, int h, u32 c) {
    SDL_SetRenderDrawBlendMode(ren, cA(c) == 255 ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, cR(c), cG(c), cB(c), cA(c));
    SDL_Rect r{x, y, w, h};
    SDL_RenderFillRect(ren, &r);
}

void Game::sprS(const Spr& s, int sx, int sy, bool flip, float alpha, double angle, bool pivot) {
    if (!s.t) return;
    int ax = flip ? s.w - 1 - s.ax : s.ax;
    SDL_Rect d{sx - ax, sy - s.ay, s.w, s.h};
    SDL_Point c{ax, s.ay};
    SDL_SetTextureAlphaMod(s.t, (Uint8)(clampf(alpha, 0, 1) * 255));
    SDL_RenderCopyEx(ren, s.t, nullptr, &d, angle, pivot ? &c : nullptr, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(s.t, 255);
}

void Game::icon(const Spr& s, int cx, int cy, int size, float alpha) {
    if (!s.t) return;
    SDL_Rect d{cx - size / 2, cy - size / 2, size, size};
    SDL_SetTextureAlphaMod(s.t, (Uint8)(clampf(alpha, 0, 1) * 255));
    SDL_RenderCopy(ren, s.t, nullptr, &d);
    SDL_SetTextureAlphaMod(s.t, 255);
}

void Game::text(int x, int y, const std::string& s, u32 col, int sc, bool shadow, float alpha) {
    int cx = x;
    int so = std::max(1, sc / 2);
    for (char ch : s) {
        int idx = glyphIndex(ch);
        SDL_Rect src{idx * 4, 0, 3, 5};
        if (shadow) {
            SDL_Rect dd{cx + so, y + so, 3 * sc, 5 * sc};
            SDL_SetTextureColorMod(A.font, 20, 12, 6);
            SDL_SetTextureAlphaMod(A.font, (Uint8)(alpha * 230));
            SDL_RenderCopy(ren, A.font, &src, &dd);
        }
        SDL_Rect d{cx, y, 3 * sc, 5 * sc};
        SDL_SetTextureColorMod(A.font, cR(col), cG(col), cB(col));
        SDL_SetTextureAlphaMod(A.font, (Uint8)(alpha * 255));
        SDL_RenderCopy(ren, A.font, &src, &d);
        cx += 4 * sc;
    }
    SDL_SetTextureColorMod(A.font, 255, 255, 255);
    SDL_SetTextureAlphaMod(A.font, 255);
}
void Game::textC(int cx, int y, const std::string& s, u32 col, int sc, bool shadow) { text(cx - textW(s, sc) / 2, y, s, col, sc, shadow); }
void Game::textOutlined(int x, int y, const std::string& s, u32 col, float alpha) {
    int cx = x;
    for (char ch : s) {
        int idx = glyphIndex(ch);
        SDL_Rect src{idx * 4, 0, 3, 5};
        SDL_SetTextureColorMod(A.font, 20, 12, 6);
        SDL_SetTextureAlphaMod(A.font, (Uint8)(alpha * 255));
        const int off[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (auto& o : off) {
            SDL_Rect d{cx + o[0], y + o[1], 3, 5};
            SDL_RenderCopy(ren, A.font, &src, &d);
        }
        SDL_Rect d{cx, y, 3, 5};
        SDL_SetTextureColorMod(A.font, cR(col), cG(col), cB(col));
        SDL_RenderCopy(ren, A.font, &src, &d);
        cx += 4;
    }
    SDL_SetTextureColorMod(A.font, 255, 255, 255);
    SDL_SetTextureAlphaMod(A.font, 255);
}

void Game::panel(int x, int y, int w, int h, u32 fill) {
    fillRect(x + 1, y, w - 2, h, UI_BORDER);
    fillRect(x, y + 1, w, h - 2, UI_BORDER);
    fillRect(x + 1, y + 1, w - 2, h - 2, fill);
    fillRect(x + 1, y + 1, w - 2, 1, shadeC(fill, 1.35f));
    fillRect(x + 1, y + 1, 1, h - 2, shadeC(fill, 1.2f));
    fillRect(x + 1, y + h - 2, w - 2, 1, shadeC(fill, 0.7f));
    fillRect(x + w - 2, y + 1, 1, h - 2, shadeC(fill, 0.8f));
}

void Game::bar(int x, int y, int w, int h, float f, u32 fill, u32 back) {
    fillRect(x - 1, y - 1, w + 2, h + 2, UI_BORDER);
    fillRect(x, y, w, h, back);
    int fw = (int)(w * clampf(f, 0, 1));
    if (fw > 0) {
        fillRect(x, y, fw, h, fill);
        fillRect(x, y, fw, 1, shadeC(fill, 1.35f));
        if (h > 2) fillRect(x, y + h - 1, fw, 1, shadeC(fill, 0.7f));
    }
}

bool Game::button(int x, int y, int w, int h, const std::string& label, bool enabled, u32 base) {
    bool hov = enabled && inRect(x, y, w, h);
    bool down = hov && in.l;
    panel(x, y + (down ? 1 : 0), w, h, !enabled ? UI_BTN_OFF : hov ? UI_BTN_HOV : base);
    textC(x + w / 2, y + (h - 5) / 2 + (down ? 1 : 0), label, enabled ? C_TEXT : HEX(0x6e5f4a));
    if (hov && in.uiClick) {
        in.uiClick = false;   // only one button may react to a click
        in.lPressed = false;  // don't also start a weapon throw
        audio.play(S_CLICK);
        return true;
    }
    return false;
}

int Game::chip(int x, int y, const Spr& ic, int have, int need) {
    icon(ic, x + 6, y + 5, 12);
    std::string t = S(have) + "/" + S(need);
    text(x + 14, y + 3, t, have >= need ? C_GREEN : C_RED);
    return x + 14 + textW(t) + 6;
}

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
    for (auto& d : decals) {
        float a = std::min(1.f, (d.life - d.t) / 8.f) * 0.9f;
        sprW(A.blood[d.size], d.p.x, d.p.y, false, a);
    }
    // thrown weapons lying on the ground
    for (auto& it : items) {
        sprW(A.shadow[0], it.pos.x, it.pos.y, false, 0.6f);
        sprW(A.wpnWorld[it.wpn], it.pos.x, it.pos.y - 2, false, 1.f, it.angle * 57.2958, true);
        float g = fmodf(timeAcc + it.pos.x * 0.01f, 1.8f);
        if (g < 0.25f) {
            int gx = (int)it.pos.x - camX + 3, gy = (int)it.pos.y - camY - 6;
            fillRect(gx, gy - 1, 1, 3, RGBA(255, 255, 255, 220));
            fillRect(gx - 1, gy, 3, 1, RGBA(255, 255, 255, 220));
        }
    }
    // carcasses
    for (auto& a : animals) {
        if (!a.dead) continue;
        if (a.pos.x < camX - 50 || a.pos.x > camX + LW + 50 || a.pos.y < camY - 50 || a.pos.y > camY + LH + 50) continue;
        const Spr& s = A.animal[a.kind][a.variant][4];
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
        sprW(A.shadow[0], n.x, n.y, false, 0.55f);
        bool plant = n.kind != RN_ORE && n.kind != RN_CACTUS;
        sprW(A.B.node[n.kind], n.x, n.y, false, 1.f, plant ? sinf(timeAcc * 1.6f + n.ph) * 3.f : 0.f, true);
        float g = fmodf(timeAcc * 0.9f + n.ph, 3.2f);
        if (g < 0.22f) {
            int gx = (int)n.x - camX + 4, gy = (int)n.y - camY - 12;
            fillRect(gx, gy - 1, 1, 3, RGBA(255, 255, 255, 220));
            fillRect(gx - 1, gy, 3, 1, RGBA(255, 255, 255, 220));
        }
    }

    // ---- y-sorted layer
    struct DL {
        float y;
        int t, i;
    };
    std::vector<DL> dl;
    for (auto it = std::lower_bound(world.props.begin(), world.props.end(), camY - 20.f, yLess); it != world.props.end() && it->y <= camY + LH + 90; ++it) {
        if (it->x < camX - 80 || it->x > camX + LW + 80) continue;
        dl.push_back({it->y, 0, (int)(it - world.props.begin())});
    }
    dl.push_back({world.keeper.y, 1, 0});
    dl.push_back({world.questGiver.y, 4, 0});
    dl.push_back({p.pos.y, 2, 0});
    for (size_t i = 0; i < animals.size(); i++)
        if (!animals[i].dead) dl.push_back({animals[i].pos.y, 3, (int)i});
    std::sort(dl.begin(), dl.end(), [](const DL& a, const DL& b) { return a.y < b.y; });

    float night = 1.f - daylight();
    for (auto& e : dl) {
        if (e.t == 0) {
            const Prop& pr = world.props[e.i];
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
            }
        } else if (e.t == 1) {
            sprW(A.shadow[0], world.keeper.x, world.keeper.y, false, 0.8f);
            sprW(A.keeper[((int)(timeAcc * 1.6f)) & 1], world.keeper.x, world.keeper.y, p.pos.x < world.keeper.x);
        } else if (e.t == 4) {
            sprW(A.shadow[0], world.questGiver.x, world.questGiver.y, false, 0.8f);
            sprW(A.quester[((int)(timeAcc * 1.3f)) & 1], world.questGiver.x, world.questGiver.y, p.pos.x < world.questGiver.x);
        } else if (e.t == 2) {
            sprW(A.shadow[0], p.pos.x, p.pos.y);
            int fr = ((int)p.animT) & 3;
            const Spr& s = A.player[p.dir][fr];
            bool fl = p.dir == 2 && p.flip;
            if (p.dead) {
                sprW(s, p.pos.x, p.pos.y, fl, 0.9f, fl ? -90 : 90, true);
            } else {
                sprW(s, p.pos.x, p.pos.y, fl);
                if (p.invuln > 0 && (((int)(timeAcc * 18)) & 1)) {
                    Spr f = s;
                    f.t = s.flash;
                    sprW(f, p.pos.x, p.pos.y, fl, 0.55f);
                }
                // weapon in hand while winding up
                if (p.charging && p.wpn[p.sel] > 0) {
                    float back = p.charge * 3.f;
                    V2 hp = p.pos + V2(0, -10) + p.aim * (7.f - back);
                    sprW(A.wpnWorld[p.sel], hp.x, hp.y, false, 1.f, atan2f(p.aim.y, p.aim.x) * 57.2958, true);
                }
                if (p.gathering) {
                    Animal* c = nearestCarcass(30);
                    if (c) {
                        float a = -50.f + sinf(timeAcc * 22.f) * 45.f;
                        sprW(A.wpnWorld[W_FLINT], c->pos.x + (p.pos.x < c->pos.x ? -5 : 5), c->pos.y - 9, false, 1.f, a + (p.pos.x < c->pos.x ? 0 : 180), true);
                    }
                }
            }
        } else {
            Animal& a = animals[e.i];
            float spd = a.vel.len();
            int fr = spd > 1 ? ((int)a.animT) & 3 : 0;
            float hop = (a.kind == A_RABBIT && spd > 1) ? fabsf(sinf(a.animT * 3.14159f * 2)) * 3.f : 0.f;
            const Spr& s = A.animal[a.kind][a.variant][fr];
            int shd = (a.kind == A_RABBIT) ? 0 : 1;
            sprW(A.shadow[shd], a.pos.x, a.pos.y, false, 0.9f);
            sprW(s, a.pos.x, a.pos.y - hop, a.face < 0);
            if (a.flash > 0) {
                Spr f = s;
                f.t = s.flash;
                sprW(f, a.pos.x, a.pos.y - hop, a.face < 0, a.flash / 0.16f * 0.85f);
            }
            if (a.hpShow > 0) {
                int bx = (int)a.pos.x - camX - 9, by = (int)a.pos.y - camY - (int)(a.bodyH * 2 + (a.kind == A_DEER ? 14 : 8));
                bar(bx, by, 18, 2, (float)a.hp / a.maxhp, HEX(0xd8443f), HEX(0x3a1a18));
            }
        }
    }

    // projectiles
    for (auto& pr : projs) {
        sprW(A.shadow[0], pr.pos.x, pr.pos.y + 9, false, 0.45f);
        float ang = (pr.angle + pr.spin * (pr.travelled / pr.speed)) * 57.2958;
        sprW(A.wpnWorld[pr.wpn], pr.pos.x, pr.pos.y, false, 1.f, ang, true);
        if (pr.crit) fillRect((int)pr.pos.x - camX, (int)pr.pos.y - camY, 1, 1, RGBA(255, 200, 80, 220));
    }
    // particles
    for (auto& q : parts) {
        float a = clampf(q.life / q.max, 0, 1);
        u32 c = withA(q.col, (int)(255 * std::min(1.f, a * 2.f)));
        fillRect((int)(q.x - camX), (int)(q.y - q.z - camY), q.size, q.size, c);
    }

    // ---- lighting: day/night tint, warm glows, vignette
    float night2 = night;
    if (night2 > 0.01f) fillRect(0, 0, LW, LH, RGBA(14, 22, 70, (int)(night2 * 128)));
    float sun = sinf(tod() * 6.2832f);
    float warm = clampf(1.f - fabsf(sun) * 3.2f, 0, 1);
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
        SDL_SetTextureAlphaMod(g, 255);
    }
    drawBiomeFx();
    // fireflies at night / pollen by day
    // (none in the desert or the snow: it is too dry / too cold for them)
    float flyW = bw[B_HOME] + bw[B_SWAMP] + bw[B_MOUNTAIN] * 0.4f;
    for (auto& f : flies) {
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

// ================================================================= HUD
// ================================================================= biome atmosphere
void Game::drawBiomeFx() {
    float day = daylight();
    auto tint = [&](int b, int r, int g, int bl, int a) {
        if (bw[b] > 0.01f) fillRect(0, 0, LW, LH, RGBA(r, g, bl, (int)(a * bw[b])));
    };
    tint(B_DESERT, 255, 196, 90, (int)(30 * (0.35f + 0.65f * day)));  // hot haze, strongest at midday
    tint(B_SWAMP, 52, 86, 60, 46);                                    // green murk
    tint(B_SNOW, 200, 225, 255, (int)(30 * (0.5f + 0.5f * day)));     // cold blue light
    tint(B_MOUNTAIN, 150, 160, 190, 20);                              // thin cool air

    // ground fog / mist: world-anchored puffs so they drift with the map instead of the screen
    float fogK = bw[B_SWAMP] + bw[B_MOUNTAIN] * 0.30f + bw[B_SNOW] * 0.22f;
    if (fogK > 0.02f) {
        float tw = bw[B_SWAMP] + bw[B_MOUNTAIN] + bw[B_SNOW] + 1e-4f;
        int cr = (int)((170 * bw[B_SWAMP] + 215 * bw[B_MOUNTAIN] + 228 * bw[B_SNOW]) / tw);
        int cg = (int)((205 * bw[B_SWAMP] + 218 * bw[B_MOUNTAIN] + 238 * bw[B_SNOW]) / tw);
        int cb = (int)((170 * bw[B_SWAMP] + 232 * bw[B_MOUNTAIN] + 252 * bw[B_SNOW]) / tw);
        SDL_SetTextureColorMod(A.B.fog.t, (Uint8)cr, (Uint8)cg, (Uint8)cb);
        const int cell = 88;
        int gx0 = (camX - 60) / cell - 1, gy0 = (camY - 40) / cell - 1, gx1 = (camX + LW + 60) / cell + 1, gy1 = (camY + LH + 40) / cell + 1;
        for (int gy = gy0; gy <= gy1; gy++)
            for (int gx = gx0; gx <= gx1; gx++) {
                uint32_t h = hash2(gx, gy, 4242);
                float px = gx * cell + (h & 63) + sinf(timeAcc * 0.11f + (h >> 8 & 255) * 0.05f) * 16.f;
                float py = gy * cell + ((h >> 6) & 31) + cosf(timeAcc * 0.09f + (h >> 4 & 255) * 0.05f) * 6.f;
                float pulse = 0.65f + 0.35f * sinf(timeAcc * 0.25f + (h >> 12 & 255) * 0.1f);
                float a = clampf(fogK * pulse * (0.55f + 0.45f * (1.f - 0.5f * day)), 0, 1);
                SDL_SetTextureAlphaMod(A.B.fog.t, (Uint8)(a * 255));
                sprS(A.B.fog, (int)px - camX, (int)py - camY);
            }
        SDL_SetTextureAlphaMod(A.B.fog.t, 255);
    }

    // weather particles
    for (auto& f : flakes) {
        int x = (int)f.x, y = (int)f.y;
        if (bw[B_SNOW] > 0.02f) fillRect(x, y, f.sz > 0.7f ? 2 : 1, f.sz > 0.7f ? 2 : 1, RGBA(255, 255, 255, (int)(235 * bw[B_SNOW])));
        if (bw[B_DESERT] > 0.02f && f.sz > 0.35f) fillRect(x, y, 2, 1, RGBA(226, 196, 130, (int)(120 * bw[B_DESERT])));
        if (bw[B_SWAMP] > 0.02f && f.sz > 0.6f) fillRect(x, y, 1, 1, RGBA(46, 50, 30, (int)(190 * bw[B_SWAMP] * (0.4f + 0.6f * day))));
        if (bw[B_MOUNTAIN] > 0.02f && f.sz > 0.5f) fillRect(x, y, 2, 1, RGBA(225, 228, 238, (int)(100 * bw[B_MOUNTAIN])));
    }
}

void Game::drawBiomeBanner() {
    if (bannerT <= 0 || state != ST_PLAY) return;
    float k = std::min(1.f, (3.2f - bannerT) * 5.f);  // slide in
    float out = std::min(1.f, bannerT * 2.f);         // and fade out
    if (out < 0.35f) return;
    const BiomeDef& B = BIOME[bannerBiome];
    std::string name = bannerBiome == B_HOME ? "THE WILDWOOD" : B.name;
    int w = std::max(textW(name, 2), textW(bannerSub)) + 24, h = bannerSub.empty() ? 22 : 33;
    int x = LW / 2 - w / 2, y = 14 - (int)((1.f - k) * 40);
    panel(x, y, w, h, HEX(0x2b1d12));
    fillRect(x + 2, y + 2, w - 4, 2, B.col);
    textC(LW / 2, y + 8, name, B.col, 2);
    if (!bannerSub.empty()) textC(LW / 2, y + 22, bannerSub, C_DIM);
}

void Game::drawPrompt(float wx, float wy, const std::string& key, const std::string& label) {
    int sx = (int)wx - camX, sy = (int)wy - camY + (int)(sinf(timeAcc * 5.f) * 1.5f);
    int w = 20 + textW(label), h = 13;
    int x = sx - w / 2, y = sy - h;
    panel(x, y, w, h, HEX(0x3a2a1a));
    panel(x + 3, y + 2, 9, 9, HEX(0x92693a));
    textC(x + 7, y + 4, key, C_TEXT, 1, false);
    text(x + 16, y + 4, label, C_TEXT);
    fillRect(sx - 1, y + h, 3, 1, UI_BORDER);
    fillRect(sx, y + h + 1, 1, 1, UI_BORDER);
}

void Game::drawMinimap() {
    int x = LW - MM_W - 10, y = 6;
    panel(x, y, MM_W + 4, MM_H + 4, HEX(0x2b1d12));
    SDL_Rect d{x + 2, y + 2, MM_W, MM_H};
    SDL_SetTextureAlphaMod(miniTex, 235);
    SDL_RenderCopy(ren, miniTex, nullptr, &d);
    int ox = x + 2, oy = y + 2;
    auto mx = [&](float wx) { return ox + (int)(wx / TS / MM_DIV); };
    auto my = [&](float wy) { return oy + (int)(wy / TS / MM_DIV); };
    // view rectangle
    u32 wc = RGBA(255, 255, 255, 120);
    int vx = mx((float)camX), vy = my((float)camY), vw = std::max(2, LW / TS / MM_DIV), vh = std::max(2, LH / TS / MM_DIV);
    fillRect(vx, vy, vw, 1, wc);
    fillRect(vx, vy + vh, vw, 1, wc);
    fillRect(vx, vy, 1, vh, wc);
    fillRect(vx + vw, vy, 1, vh + 1, wc);
    // shop and elder
    fillRect(mx(world.hut.x) - 1, my(world.hut.y) - 1, 3, 3, C_GOLD);
    fillRect(mx(world.questGiver.x), my(world.questGiver.y), 2, 2, C_BLUE);
    // discovered landmarks, in their biome's colour
    for (auto& l : world.landmarks)
        if (l.found) fillRect(mx(l.x) - 1, my(l.y) - 1, 2, 2, BIOME[l.biome].col);
    // carcasses that still hold materials
    for (auto& a : animals)
        if (a.dead && a.totalRemain > 0) fillRect(mx(a.pos.x), my(a.pos.y), 2, 2, C_RED);
    // player
    if (((int)(timeAcc * 3)) & 1 || state != ST_PLAY) fillRect(mx(p.pos.x) - 1, my(p.pos.y) - 1, 3, 3, HEX(0xffffff));
    fillRect(mx(p.pos.x), my(p.pos.y), 1, 1, HEX(0x2d5e35));
    // where am I
    const BiomeDef& B = BIOME[curBiome];
    text(x + 2, y + MM_H + 8, curBiome == B_HOME ? "WILDWOOD" : B.name, B.col);
    if (B.climate[0]) text(x + 2, y + MM_H + 15, B.climate, C_DIM);
    text(x + MM_W - textW(S(landmarksFound()) + "/" + S((int)world.landmarks.size())) + 2, y + MM_H + 8, S(landmarksFound()) + "/" + S((int)world.landmarks.size()), C_DIM);
}

void Game::drawHud() {
    if (state == ST_TITLE) return;
    // ---- floating marker over the elder: "!" = quest available, "?" = ready to turn in
    if (!questActive || questReady()) {
        int qx = (int)world.questGiver.x - camX, qy = (int)world.questGiver.y - camY - 38 + (int)(sinf(timeAcc * 4.f) * 1.5f);
        if (qx > -10 && qx < LW + 10 && qy > -12 && qy < LH + 12)
            text(qx - 3, qy, questActive ? "?" : "!", questActive ? C_GREEN : C_GOLD, 2);
    }
    // ---- context prompts in the world
    if (state == ST_PLAY && !p.dead) {
        Animal* c = nearestCarcass(26);
        if (c) {
            int sx = (int)c->pos.x - camX, sy = (int)c->pos.y - camY;
            // remaining materials
            int n = 0;
            for (int m = 0; m < M_COUNT; m++)
                if (c->remain[m] > 0) n++;
            int pw = n * 20 + 6;
            int px = sx - pw / 2, py = sy - 46;
            panel(px, py, pw, 21, HEX(0x3a2a1a));
            int cx = px + 3;
            for (int m = 0; m < M_COUNT; m++) {
                if (c->remain[m] <= 0) continue;
                icon(A.matIcon[m], cx + 8, py + 8, 14);
                text(cx + 12, py + 13, S(c->remain[m]), C_TEXT);
                cx += 20;
            }
            if (p.gathering) {
                float f = p.gatherT / ANIMAL[c->kind].gatherTime;
                bar(sx - 14, sy - 22, 28, 4, f, C_GREEN);
            } else
                drawPrompt(c->pos.x, c->pos.y - 22, "E", "HOLD TO GATHER");
        } else if (nearShop()) {
            drawPrompt(world.keeper.x, world.keeper.y - 36, "E", "SHOP");
        } else if (nearQuestGiver()) {
            drawPrompt(world.questGiver.x, world.questGiver.y - 44, "E", "TALK");
        }
    }

    // ---- top-left vitals
    panel(4, 4, 98, 40, HEX(0x33241a));
    icon(A.heart, 12, 12, 11);
    float hf = p.hp / p.maxhp;
    bar(21, 9, 74, 7, hf, hf > 0.5f ? HEX(0x59b543) : hf > 0.25f ? HEX(0xe0a93a) : HEX(0xd8443f));
    text(23, 10, S((int)ceilf(p.hp)) + "/" + S((int)p.maxhp), C_TEXT, 1, true);
    icon(A.bolt, 12, 24, 9);
    bar(21, 21, 74, 4, p.stamina / 100.f, p.exhausted ? HEX(0xa86a2a) : HEX(0xe0c43a));
    icon(A.coin, 12, 35, 10);
    text(21, 33, S(p.coins), C_GOLD);

    // ---- clock
    {
        float t = tod();
        int mins = (int)(fmodf(6.f + t * 24.f, 24.f) * 60);
        char buf[32];
        snprintf(buf, sizeof(buf), "DAY %d  %02d:%02d", 1 + (int)(dayT / 300.f), mins / 60, mins % 60);
        int w = textW(buf) + 18;
        int x = LW / 2 - w / 2 - 8;
        panel(x, 4, w, 13, HEX(0x33241a));
        bool dayT_ = daylight() > 0.4f;
        fillRect(x + 5, 8, 5, 5, dayT_ ? C_GOLD : HEX(0xcfd8e8));
        if (!dayT_) fillRect(x + 7, 8, 3, 3, HEX(0x33241a));
        text(x + 13, 8, buf, C_TEXT);
    }
    // ---- messages
    {
        int y = 21;
        for (auto& m : msgs) {
            float a = std::min(1.f, m.t / 0.6f);
            textC(LW / 2 - 8, y, m.s, m.col, 1, true);
            (void)a;
            y += 8;
        }
    }
    if (showMap) drawMinimap();
    drawBiomeBanner();

    // ---- active quest tracker (bottom-left, above the bag meter)
    if (questActive) {
        bool ready = questReady();
        std::string t = ready ? quest.title + "  DONE! SEE ELDER" : quest.title + "  " + S(questProg) + "/" + S(quest.goal);
        panel(4, LH - 59, textW(t) + 8, 12, HEX(0x33241a));
        text(8, LH - 56, t, ready ? C_GREEN : C_TEXT, 1, false);
    }

    // ---- materials strip + bag meter
    {
        int x = 4, y = LH - 29;
        int used = p.bagUsed(), cap = p.bagCap();
        panel(x, y, M_COUNT * 18 + 6, 25, HEX(0x33241a));
        for (int m = 0; m < M_COUNT; m++) {
            int cx = x + 4 + m * 18;
            icon(A.matIcon[m], cx + 8, y + 9, 14, p.mats[m] > 0 ? 1.f : 0.35f);
            std::string t = S(p.mats[m]);
            textC(cx + 8, y + 17, t, p.mats[m] > 0 ? C_TEXT : HEX(0x6e5f4a), 1, false);
        }
        float f = (float)used / cap;
        text(x + 1, y - 9, "BAG " + S(used) + "/" + S(cap), used >= cap ? C_RED : C_TEXT);
        bar(x + 52, y - 8, 40, 3, f, used >= cap ? HEX(0xd8443f) : HEX(0x8ec2f0));
    }
    // ---- hotbar
    {
        int sw = 24, gap = 3, total = W_COUNT * sw + (W_COUNT - 1) * gap;
        int x0 = (LW - total) / 2 + 14, y = LH - sw - 5;
        for (int i = 0; i < W_COUNT; i++) {
            int x = x0 + i * (sw + gap);
            bool sel = i == p.sel;
            int yy = y - (sel ? 3 : 0);
            panel(x, yy, sw, sw, sel ? HEX(0x6a4a2a) : HEX(0x33241a));
            if (sel) {
                u32 g = C_GOLD;
                fillRect(x, yy + 1, 1, sw - 2, g);
                fillRect(x + sw - 1, yy + 1, 1, sw - 2, g);
                fillRect(x + 1, yy, sw - 2, 1, g);
                fillRect(x + 1, yy + sw - 1, sw - 2, 1, g);
            }
            icon(A.wpnIcon[i], x + sw / 2, yy + sw / 2, 16, p.wpn[i] > 0 ? 1.f : 0.3f);
            text(x + 3, yy + 3, S(i + 1), C_DIM, 1, false);
            std::string n = S(p.wpn[i]);
            text(x + sw - 3 - textW(n), yy + sw - 8, n, p.wpn[i] > 0 ? C_TEXT : C_RED);
            if (sel && p.cd > 0) bar(x + 3, yy + sw - 3, sw - 6, 1, 1.f - p.cd / WPN[i].cd, C_GOLD);
        }
        int bx = x0 + total + 7;
        panel(bx, y, sw, sw, HEX(0x33241a));
        icon(A.bandage, bx + sw / 2, y + sw / 2, 16, p.bandages > 0 ? 1.f : 0.3f);
        text(bx + 3, y + 3, "Q", C_DIM, 1, false);
        std::string n = S(p.bandages);
        text(bx + sw - 3 - textW(n), y + sw - 8, n, p.bandages > 0 ? C_TEXT : C_RED);
        // selected weapon name
        textC(x0 + total / 2, y - 12, WPN[p.sel].name, C_TEXT);
    }
    // ---- help card
    if (helpT > 0 && state == ST_PLAY) {
        const char* lines[] = {"WASD MOVE    SHIFT SPRINT    1-4 OR WHEEL: WEAPON", "HOLD LEFT CLICK TO CHARGE - RELEASE TO THROW",
                               "WALK OVER THROWN WEAPONS TO PICK THEM UP", "HOLD E AT A CARCASS TO GATHER - E AT THE SHOP TO TRADE",
                               "Q BANDAGE   M MAP   H HELP   ESC PAUSE"};
        int w = 232, h = 52;
        int x = LW / 2 - w / 2, y = 52;
        panel(x, y, w, h, HEX(0x33241a));
        textC(LW / 2, y + 5, "HOW TO HUNT", C_GOLD);
        for (int i = 0; i < 5; i++) textC(LW / 2, y + 14 + i * 7, lines[i], C_TEXT, 1, false);
    }
    // ---- knocked out
    if (p.dead) {
        float a = clampf(p.deadT / 0.8f, 0, 1);
        fillRect(0, 0, LW, LH, RGBA(10, 4, 4, (int)(a * 170)));
        textC(LW / 2, LH / 2 - 14, "KNOCKED OUT", C_RED, 3);
        textC(LW / 2, LH / 2 + 8, "WAKING UP BY THE CAMPFIRE...", C_TEXT);
    }
}

void Game::drawReticle() {
    int mx = (int)in.mx, my = (int)in.my;
    bool over = false;
    for (auto& a : animals)
        if (!a.dead && dist(V2(in.mx + camX, in.my + camY), a.pos + V2(0, -a.bodyH)) < ANIMAL[a.kind].hitR + 4) over = true;
    u32 col = over ? HEX(0xff6a5a) : HEX(0xffffff);
    u32 sh = HEX(0x140d07);
    // aim line while charging
    if (p.charging) {
        V2 hand = p.pos + V2(0, -10);
        V2 mw(in.mx + camX, in.my + camY);
        V2 d = mw - hand;
        float dl = d.len();
        if (dl > 6) {
            V2 dir = d * (1.f / dl);
            const WpnDef& W = WPN[p.sel];
            float range = std::min(W.range * (1 + 0.15f * p.charge), dl + 24);
            for (float t = 14; t < range; t += 6) {
                V2 q = hand + dir * t;
                fillRect((int)q.x - camX, (int)q.y - camY, 1, 1, RGBA(255, 255, 255, (int)(190 * (1.f - t / (range + 20)))));
            }
            V2 e = hand + dir * range;
            int ex = (int)e.x - camX, ey = (int)e.y - camY;
            fillRect(ex - 2, ey, 5, 1, RGBA(255, 220, 120, 220));
            fillRect(ex, ey - 2, 1, 5, RGBA(255, 220, 120, 220));
        }
    }
    auto tick = [&](int x, int y, int w, int h) {
        fillRect(x - 1, y - 1, w + 2, h + 2, sh);
        fillRect(x, y, w, h, col);
    };
    int g = p.charging ? 4 - (int)(p.charge * 2) : 5;
    tick(mx - g - 3, my, 3, 1);
    tick(mx + g + 1, my, 3, 1);
    tick(mx, my - g - 3, 1, 3);
    tick(mx, my + g + 1, 1, 3);
    fillRect(mx, my, 1, 1, col);
    if (p.charging) {
        int n = (int)(p.charge * 24);
        for (int i = 0; i < n; i++) {
            float a = -1.5708f + i * 6.2832f / 24.f;
            fillRect(mx + (int)roundf(cosf(a) * 10) , my + (int)roundf(sinf(a) * 10), 1, 1, p.charge >= 1 ? C_GOLD : HEX(0xffffff));
        }
    }
}

// ================================================================= quest giver
void Game::drawQuestPanel() {
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 140));
    const int X = 60, Y = 36, W = 280, H = 146;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, UI_BG);
    text(X + 8, Y + 7, "ELDER HOLT - HUNTING QUESTS", C_GOLD);
    std::string qn = "QUEST " + S(questIdx + 1);
    text(X + W - 8 - textW(qn), Y + 7, qn, C_DIM);
    // portrait of the elder
    panel(X + 6, Y + 22, 60, 78, HEX(0x2b1d12));
    {
        const Spr& k = A.quester[((int)(timeAcc * 1.3f)) & 1];
        SDL_Rect d{X + 6 + 30 - k.w, Y + 26, k.w * 2, k.h * 2};
        SDL_RenderCopy(ren, k.t, nullptr, &d);
        textC(X + 36, Y + 88, "ELDER HOLT", C_DIM, 1, false);
    }
    // the quest itself
    const int x0 = X + 74;
    bool ready = questReady();
    text(x0, Y + 23, quest.title, C_GOLD, 2);
    text(x0, Y + 38, quest.desc, C_TEXT);
    float frac = questActive ? (float)questProg / quest.goal : 0.f;
    bar(x0, Y + 52, 150, 6, frac, ready ? C_GREEN : HEX(0xe0a93a));
    text(x0 + 156, Y + 52, questActive ? S(questProg) + "/" + S(quest.goal) : "GOAL " + S(quest.goal), ready ? C_GREEN : C_TEXT);
    // rewards
    text(x0, Y + 66, "REWARD", C_DIM, 1, false);
    int cx = x0;
    std::string cs = S(quest.coins);
    icon(A.coin, cx + 5, Y + 79, 10);
    text(cx + 12, Y + 76, cs, C_GOLD);
    cx += 12 + textW(cs) + 8;
    if (quest.mat >= 0 && quest.matN > 0) {
        std::string ms = "X" + S(quest.matN);
        icon(A.matIcon[quest.mat], cx + 6, Y + 79, 12);
        text(cx + 14, Y + 76, ms, C_TEXT);
        cx += 14 + textW(ms) + 8;
    }
    if (quest.bandages > 0) {
        std::string bs = "X" + S(quest.bandages);
        icon(A.bandage, cx + 6, Y + 79, 12);
        text(cx + 14, Y + 76, bs, C_TEXT);
    }
    // status line + buttons
    if (!questActive) text(x0, Y + 90, "ACCEPT TO START THIS QUEST", C_DIM, 1, false);
    else if (ready) text(x0, Y + 90, "WELL DONE, HUNTER!", C_GREEN, 1, false);
    else text(x0, Y + 90, "KEEP HUNTING - COME BACK WHEN DONE", C_DIM, 1, false);
    const int by = Y + 106;
    if (!questActive) {
        if (button(x0, by, 96, 18, "ACCEPT", true, HEX(0x5d7a2e))) acceptQuest();
    } else if (ready) {
        if (button(x0, by, 96, 18, "TURN IN", true, HEX(0x5d7a2e))) turnInQuest();
    } else {
        button(x0, by, 96, 18, "IN PROGRESS", false);
    }
    if (button(x0 + 102, by, 60, 18, "CLOSE")) {
        state = ST_PLAY;
        save();
    }
    textC(X + W / 2, Y + H - 12, "ESC OR E: CLOSE", C_DIM, 1, false);
}

// ================================================================= shop
void Game::drawShop() {
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 140));
    const int X = 36, Y = 12, W = 328, H = 201;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, UI_BG);
    text(X + 8, Y + 7, "BRAMBLE TRADING POST", C_GOLD);
    {
        std::string c = S(p.coins);
        int cw = textW(c);
        icon(A.coin, X + W - 10 - cw - 8, Y + 10, 10);
        text(X + W - 8 - cw, Y + 7, c, C_GOLD);
    }
    const char* tabs[4] = {"SELL", "CRAFT", "UPGRADE", "BUY"};
    for (int i = 0; i < 4; i++) {
        int ty = Y + 22 + i * 22;
        bool sel = shopTab == i;
        if (button(X + 6, ty, 66, 19, tabs[i], true, sel ? HEX(0x92693a) : HEX(0x5a4028))) shopTab = i;
        if (sel) {
            fillRect(X + 72, ty + 3, 3, 13, C_GOLD);
        }
    }
    // shopkeeper portrait
    panel(X + 6, Y + 112, 66, 72, HEX(0x2b1d12));
    {
        const Spr& k = A.keeper[((int)(timeAcc * 1.6f)) & 1];
        SDL_Rect d{X + 6 + 33 - k.w, Y + 114, k.w * 2, k.h * 2};
        SDL_RenderCopy(ren, k.t, nullptr, &d);
        textC(X + 39, Y + 168, "MERCHANT", C_DIM, 1, false);
        const char* lines[] = {"GOOD HUNTING!", "FRESH HIDES?", "I PAY WELL!", "SHARP KNIVES!"};
        textC(X + 39, Y + 176, lines[((int)(titleT + timeAcc * 0.15f)) % 4], C_TEXT, 1, false);
    }
    const int x0 = X + 80, cw = 240;
    // ---------------- SELL
    if (shopTab == 0) {
        text(x0 + 20, Y + 22, "ITEM", C_DIM, 1, false);
        text(x0 + 82, Y + 22, "HAVE", C_DIM, 1, false);
        text(x0 + 112, Y + 22, "EACH", C_DIM, 1, false);
        int total = 0;
        for (int m = 0; m < M_COUNT; m++) total += p.mats[m] * MAT_PRICE[m];
        for (int m = 0; m < M_COUNT; m++) {
            int y = Y + 30 + m * 17;
            bool has = p.mats[m] > 0;
            fillRect(x0, y, cw, 16, (m & 1) ? RGBA(0, 0, 0, 40) : RGBA(0, 0, 0, 80));
            icon(A.matIcon[m], x0 + 10, y + 8, 14, has ? 1.f : 0.4f);
            text(x0 + 21, y + 5, MAT_NAME[m], has ? C_TEXT : HEX(0x7a6a52));
            text(x0 + 84, y + 5, S(p.mats[m]), has ? C_TEXT : HEX(0x7a6a52));
            icon(A.coin, x0 + 114, y + 8, 8);
            text(x0 + 120, y + 5, S(MAT_PRICE[m]), C_GOLD);
            if (button(x0 + 138, y + 1, 46, 14, "SELL 1", has)) sellMat(m, 1);
            if (button(x0 + 188, y + 1, 50, 14, "SELL ALL", has)) sellMat(m, p.mats[m]);
        }
        std::string lab = total > 0 ? "SELL EVERYTHING  +" + S(total) + " COINS" : "NOTHING TO SELL - GO HUNT!";
        if (button(x0, Y + 30 + 7 * 17 + 3, cw, 17, lab, total > 0, HEX(0x5d7a2e))) {
            for (int m = 0; m < M_COUNT; m++) sellMat(m, p.mats[m]);
            msg("SOLD EVERYTHING FOR " + S(total) + " COINS", C_GOLD);
            save();
        }
    }
    // ---------------- CRAFT
    if (shopTab == 1) {
        for (int r = 0; r < RECIPE_N; r++) {
            const Recipe& R = RECIPES[r];
            int y = Y + 22 + r * 53;
            panel(x0, y, cw, 49, HEX(0x3a2a1a));
            icon(A.wpnIcon[R.out], x0 + 20, y + 20, 32);
            text(x0 + 42, y + 5, std::string(WPN[R.out].name) + "  X" + S(R.count), C_GOLD);
            text(x0 + 42, y + 14, WPN[R.out].blurb, C_DIM);
            text(x0 + 42, y + 22, "DMG " + S((int)WPN[R.out].dmg) + "  RANGE " + S((int)WPN[R.out].range) + "  OWNED " + S(p.wpn[R.out]), C_TEXT);
            int cx = x0 + 42;
            bool ok = true;
            for (int i = 0; i < 3; i++)
                if (R.mat[i] >= 0) {
                    cx = chip(cx, y + 33, A.matIcon[R.mat[i]], p.mats[R.mat[i]], R.n[i]);
                    if (p.mats[R.mat[i]] < R.n[i]) ok = false;
                }
            if (button(x0 + 186, y + 29, 50, 16, "CRAFT", ok, HEX(0x5d7a2e))) craft(r);
        }
        text(x0, Y + 22 + RECIPE_N * 53 + 2, "WEAPONS ARE THROWN - WALK OVER THEM TO PICK THEM UP AGAIN", C_DIM, 1, false);
    }
    // ---------------- UPGRADE
    if (shopTab == 2) {
        for (int u = 0; u < U_COUNT; u++) {
            int y = Y + 22 + u * 40;
            int lv = p.up[u];
            panel(x0, y, cw, 37, HEX(0x3a2a1a));
            icon(A.gearIcon[u][std::min(2, lv)], x0 + 18, y + 19, 28, lv >= 2 ? 0.8f : 1.f);
            text(x0 + 36, y + 4, UPS[u].title, C_DIM);
            for (int i = 0; i < 2; i++) {
                fillRect(x0 + cw - 20 + i * 8, y + 4, 6, 5, i < lv ? C_GOLD : HEX(0x1a120b));
            }
            if (lv >= 2) {
                text(x0 + 36, y + 13, UPS[u].lv[1].name, C_GOLD);
                text(x0 + 36, y + 22, UPS[u].lv[1].desc, C_TEXT);
                text(x0 + 36, y + 30, "FULLY UPGRADED", C_GREEN);
                continue;
            }
            const UpLevel& L = UPS[u].lv[lv];
            text(x0 + 36, y + 13, L.name, C_GOLD);
            text(x0 + 36, y + 21, L.desc, C_TEXT);
            int cx = x0 + 36;
            cx = chip(cx, y + 27, A.coin, p.coins, L.coins);
            if (L.m1 >= 0) cx = chip(cx, y + 27, A.matIcon[L.m1], p.mats[L.m1], L.n1);
            if (L.m2 >= 0) cx = chip(cx, y + 27, A.matIcon[L.m2], p.mats[L.m2], L.n2);
            bool ok = canAfford(L.coins, L.m1, L.n1, L.m2, L.n2);
            if (button(x0 + 188, y + 17, 46, 16, "BUY", ok, HEX(0x5d7a2e))) buyUpgrade(u);
        }
    }
    // ---------------- BUY
    if (shopTab == 3) {
        struct Good {
            const char* name;
            const char* desc;
            int price;
            int kind;  // 0 flint x3, 1 bandage x1, 2 bandage x3
        } goods[3] = {{"FLINT KNIFE X3", "CHEAP THROWING KNIVES", 21, 0}, {"BANDAGE X1", "HEALS 45 HP - PRESS Q", 15, 1}, {"BANDAGE X3", "SAVE A FEW COINS", 40, 2}};
        for (int i = 0; i < 3; i++) {
            const Good& g = goods[i];
            int y = Y + 22 + i * 42;
            panel(x0, y, cw, 38, HEX(0x3a2a1a));
            icon(g.kind == 0 ? A.wpnIcon[W_FLINT] : A.bandage, x0 + 18, y + 19, 28);
            text(x0 + 36, y + 6, g.name, C_GOLD);
            text(x0 + 36, y + 15, g.desc, C_DIM);
            int owned = g.kind == 0 ? p.wpn[W_FLINT] : p.bandages;
            text(x0 + 36, y + 24, "OWNED " + S(owned), C_TEXT);
            chip(x0 + 90, y + 21, A.coin, p.coins, g.price);
            bool full = g.kind == 0 ? p.wpn[W_FLINT] + 3 > WPN_MAX : p.bandages + (g.kind == 1 ? 1 : 3) > BANDAGE_MAX;
            bool ok = p.coins >= g.price && !full;
            if (button(x0 + 186, y + 11, 48, 16, full ? "FULL" : "BUY", ok, HEX(0x5d7a2e))) {
                p.coins -= g.price;
                if (g.kind == 0) p.wpn[W_FLINT] += 3;
                else p.bandages += g.kind == 1 ? 1 : 3;
                audio.play(S_COIN);
            }
        }
    }
    textC(X + W / 2, Y + H - 10, "ESC OR E: LEAVE     TAB OR 1-4: SWITCH TAB     BAG " + S(p.bagUsed()) + "/" + S(p.bagCap()), C_DIM, 1, false);
}

// ================================================================= menus
void Game::drawTitle() {
    fillRect(0, 0, LW, LH, RGBA(10, 8, 20, 70));
    fillRect(0, 0, LW, 70, RGBA(10, 8, 20, 60));
    int bob = (int)(sinf(titleT * 1.6f) * 2);
    textC(LW / 2, 22 + bob, "WILDWOOD", HEX(0xf6d96a), 6);
    textC(LW / 2, 56 + bob, "HUNTER", HEX(0xf3e9d2), 6);
    textC(LW / 2, 92, "A TOP-DOWN HUNTING ADVENTURE", C_TEXT);
    int bx = LW / 2 - 55, by = 116;
    if (button(bx, by, 110, 20, "NEW GAME", true, HEX(0x5d7a2e))) {
        newGame();
        state = ST_PLAY;
    }
    by += 25;
    if (button(bx, by, 110, 20, "CONTINUE", hasSave)) {
        if (load()) state = ST_PLAY;
    }
    by += 25;
    if (button(bx, by, 110, 20, "QUIT", true, HEX(0x7a3a2a))) running = false;
    textC(LW / 2, LH - 22, "HUNT  GATHER  CRAFT  TRADE", C_GOLD, 1);
    textC(LW / 2, LH - 12, "WASD MOVE   MOUSE AIM   LMB THROW   E INTERACT", C_DIM, 1);
}

void Game::drawPause() {
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 150));
    panel(LW / 2 - 90, 40, 180, 140, HEX(0x4a3524));
    textC(LW / 2, 50, "PAUSED", C_GOLD, 3);
    int bx = LW / 2 - 60, by = 80;
    if (button(bx, by, 120, 18, "RESUME", true, HEX(0x5d7a2e))) state = ST_PLAY;
    by += 23;
    if (button(bx, by, 120, 18, audio.muted ? "SOUND: OFF" : "SOUND: ON")) audio.muted = !audio.muted;
    by += 23;
    if (button(bx, by, 120, 18, "SAVE AND MAIN MENU", true, HEX(0x7a3a2a))) {
        save();
        state = ST_TITLE;
    }
    textC(LW / 2, by + 28, "KILLS " + S(kills) + "    COINS EARNED " + S(sold), C_DIM, 1, false);
    textC(LW / 2, by + 38, "F11: FULLSCREEN", C_DIM, 1, false);
}

void Game::render() {
    SDL_SetRenderTarget(ren, canvas);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    renderWorld(true);
    switch (state) {
        case ST_TITLE: drawTitle(); break;
        case ST_PLAY: drawHud(); break;
        case ST_SHOP: drawHud(); drawShop(); break;
        case ST_PAUSE: drawHud(); drawPause(); break;
        case ST_QUEST: drawHud(); drawQuestPanel(); break;
    }
    if (state == ST_PLAY && !p.dead) drawReticle();
    else sprS(A.cursor, (int)in.mx, (int)in.my);
}

void Game::present() {
    SDL_SetRenderTarget(ren, nullptr);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    int ww, wh;
    SDL_GetRendererOutputSize(ren, &ww, &wh);
    float sc = std::min((float)ww / LW, (float)wh / LH);
    if (sc >= 1.f) sc = floorf(sc);
    int dw = (int)(LW * sc), dh = (int)(LH * sc);
    offX = (float)((ww - dw) / 2);
    offY = (float)((wh - dh) / 2);
    presScale = sc;
    SDL_Rect dst{(int)offX, (int)offY, dw, dh};
    SDL_RenderCopy(ren, canvas, nullptr, &dst);
    SDL_RenderPresent(ren);
}

void Game::saveCanvas(const char* path) {
    SDL_SetRenderTarget(ren, canvas);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, LW, LH, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
}
