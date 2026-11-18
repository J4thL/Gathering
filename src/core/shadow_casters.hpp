// shadow_casters.hpp - WHICH things cast a ground shadow, and what kind. Data only: the drawing is in game_light.cpp (drawGroundShadows).
//
// ======================= HOW TO ADD A SHADOW =======================
// A new thing in the world needs ONE line in one of the three tables below. You do not touch game_light.cpp.
//
//   1. A world prop (a new PropKind)          -> add a line to propShadowTable()
//   2. A placed structure (a new PlaceId)     -> add a line to structShadowTable()
//   3. A homestead piece (a new HP_ kind)     -> add a line to homeShadowTable()
//
// Pick the SHAPE that matches how the thing stands:
//   STANDING(kind, sprite [, aMul, lenMul])      a small footprint: post, cactus, barrel, gravestone, lamp, a trunk with no wide roots.
//                                                The shadow is rooted on a little oval under the sprite and leans away from the sun.
//   WIDE(kind, sprite [, aMul, lenMul])          a flat base: hut, wall, arch, bench, well. The shadow's base stays on the sprite's base line.
//   COLS(kind, sprite [, aMul, lenMul])          cast column by column from the sprite's own bottom edge: rocks, buildings, anything
//                                                whose bottom edge is not a straight line.
//   TRUNK(kind, sprite, aMul, fw, fh, minW, minH [, flip])   a tree on a trunk: oval = fw/fh * sprite width (at least minW/minH px).
//   (the big oaks have their own footprint table and are special-cased: see SH_OAK)
//
//   sprite  : a small function that returns the Spr to cast. For a plain sprite:   SPR(A.hut)
//             For an animated or per-instance one use the context: SPR(A.pine[c.frame(PINE_FPS, PINE_FRAMES)])
//             (props: `c.A`, `c.pr`, `c.frame(fps, n)`; homestead pieces: `c.A`, `c.pc`, `c.g`.)
//   aMul    : darkness of this shadow, 0..1 (1 = as dark as a tree's; 0.55 = pale glass)
//   lenMul  : how long it is compared with a tree's shadow (1 = a tree; 0.7 = a low building)
//   yOff    : (props) pixels added to the foot's y, which is prop.y + 1 on screen. Most things use -1 (the sprite's last row is prop.y itself);
//             trees and palms use 0.
//
// Homestead pieces also say where the foot is inside the piece's rectangle: HFOOT{ox, ow, oy, oh} means
//   x = piece left + ox + ow * piece width,  y = piece top + oy + oh * piece height   (all in pixels).
//
// The root of the shadow (STANDING, TRUNK, OAK) is the object's own base flattened onto the ground. To change that for one object, wrap its
// line in withFoot(line, FOOT_NONE) or withFoot(line, FOOT_OVAL), see the comment above the tables.
//
// That is all. A thing with NO line casts no shadow (resource nodes, the campfire, fences...).
// ===================================================================
#pragma once
#include <vector>
#include "core/game.hpp"
#include "world/world.hpp"
#include "items/items.hpp"
#include "farming/homestead.hpp"

namespace shadowdef {

enum Shape { SH_STANDING, SH_WIDE, SH_COLS, SH_TRUNK, SH_OAK };

// How the root of a STANDING / TRUNK / OAK shadow is drawn (wide and cols shapes have no footprint):
enum FootKind {
    FOOT_BASE,  // the object's own base: its lowest pixel rows flattened onto the ground (big oaks: the roots, swept up to the canopy shadow). Default.
    FOOT_NONE,  // nothing: the shadow just starts at the foot
    FOOT_OVAL   // the old look: a soft oval under the object
};

// ---- what a sprite picker gets to look at
struct PropCtx {
    const Assets& A;
    const Prop& pr;
    float timeAcc;
    int frame(float fps, int n) const { return ((int)(timeAcc * fps + pr.ph * n / 6.2832f) % n + n) % n; }
};
struct HomeCtx {
    Game& g;
    const Assets& A;
    const HPiece& pc;
};
struct StructCtx {
    const Assets& A;
};

// ---- one line of a table
struct PropShadow {
    int kind;
    Shape shape;
    const Spr& (*spr)(const PropCtx&);
    float aMul, lenMul, yOff;
    float fw, fh, minW, minH;       // TRUNK: footprint oval
    bool (*flip)(const Prop&);      // TRUNK: mirror the picture (some trees are drawn mirrored)
    int foot = FOOT_BASE;           // see FootKind
};
struct StructShadow {
    int kind;
    Shape shape;
    const Spr& (*spr)(const StructCtx&);
    float aMul, lenMul;
    int foot = FOOT_BASE;
};
struct HFoot {
    float ox, ow, oy, oh;
};
struct HomeShadow {
    int kind;
    Shape shape;
    const Spr& (*spr)(const HomeCtx&);
    HFoot foot;
    float aMul, lenMul;
    int footKind = FOOT_BASE;
};

// ---- helpers that keep the table lines short
#define SPR_P(expr) +[](const shadowdef::PropCtx& c) -> const Spr& { (void)c; return expr; }
#define SPR_S(expr) +[](const shadowdef::StructCtx& c) -> const Spr& { (void)c; return expr; }
#define SPR_H(expr) +[](const shadowdef::HomeCtx& c) -> const Spr& { (void)c; return expr; }

inline PropShadow pStanding(int k, const Spr& (*s)(const PropCtx&), float a = 0.9f, float l = 1.f, float y = -1.f) {
    return {k, SH_STANDING, s, a, l, y, 0, 0, 0, 0, nullptr};
}
inline PropShadow pWide(int k, const Spr& (*s)(const PropCtx&), float a = 0.8f, float l = 0.7f, float y = -1.f) {
    return {k, SH_WIDE, s, a, l, y, 0, 0, 0, 0, nullptr};
}
inline PropShadow pCols(int k, const Spr& (*s)(const PropCtx&), float a, float l, float y) {
    return {k, SH_COLS, s, a, l, y, 0, 0, 0, 0, nullptr};
}
inline PropShadow pTrunk(int k, const Spr& (*s)(const PropCtx&), float a, float fw, float fh, float minW, float minH, bool (*flip)(const Prop&) = nullptr, float y = 0.f) {
    return {k, SH_TRUNK, s, a, 1.f, y, fw, fh, minW, minH, flip};
}
inline PropShadow pOak(int k) { return {k, SH_OAK, nullptr, 1.f, 1.f, 0.f, 0, 0, 0, 0, nullptr}; }

inline StructShadow sStanding(int k, const Spr& (*s)(const StructCtx&), float a = 0.9f, float l = 1.f) { return {k, SH_STANDING, s, a, l}; }

inline HomeShadow hStanding(int k, const Spr& (*s)(const HomeCtx&), HFoot f, float a = 0.9f, float l = 1.f) { return {k, SH_STANDING, s, f, a, l}; }
inline HomeShadow hWide(int k, const Spr& (*s)(const HomeCtx&), HFoot f, float a = 0.8f, float l = 0.7f) { return {k, SH_WIDE, s, f, a, l}; }

// How the root of a shadow looks can be changed per line: wrap the line in withFoot(...), for example
//     withFoot(pStanding(P_LAMP, SPR_P(c.A.T.lamp), 0.85f), FOOT_NONE)     a lamp whose shadow just starts at the foot
//     withFoot(pStanding(P_CRATE, SPR_P(c.A.crate), 0.8f), FOOT_OVAL)      the old soft oval
// (no wrapper = FOOT_BASE: the object's own base, flattened onto the ground)
inline PropShadow withFoot(PropShadow d, FootKind f) { d.foot = f; return d; }
inline StructShadow withFoot(StructShadow d, FootKind f) { d.foot = f; return d; }
inline HomeShadow withFoot(HomeShadow d, FootKind f) { d.footKind = f; return d; }

// =========================== THE TABLES ===========================

// ---- world props
inline const std::vector<PropShadow>& propShadowTable() {
    static const std::vector<PropShadow> T = {
        // trees. The big oaks stand on wide roots, their footprint comes from OAK_VAR (the oak's own table)
        pOak(P_OAK0), pOak(P_OAK1),
        pTrunk(P_PINE, SPR_P(c.A.pine[c.frame(PINE_FPS, PINE_FRAMES)]), 1.f, 0.10f, 0.045f, 4.f, 2.f),
        pTrunk(P_SNOWPINE, SPR_P(c.A.snowSpruce[c.frame(SNOW_SPRUCE_FPS, SNOW_SPRUCE_FRAMES)]), 1.f, 0.10f, 0.045f, 4.f, 2.f),
        pTrunk(P_DEADTREE0, SPR_P(c.A.deadSpruce[c.frame(DEAD_SPRUCE_FPS, DEAD_SPRUCE_FRAMES)]), 0.8f, 0.09f, 0.04f, 3.f, 2.f),
        pTrunk(P_DEADTREE1, SPR_P(c.A.deadSpruce[c.frame(DEAD_SPRUCE_FPS, DEAD_SPRUCE_FRAMES)]), 0.8f, 0.09f, 0.04f, 3.f, 2.f, [](const Prop&) { return true; }),
        pTrunk(P_BARETREE, SPR_P(c.A.deadSpruce[c.frame(DEAD_SPRUCE_FPS, DEAD_SPRUCE_FRAMES)]), 0.8f, 0.09f, 0.04f, 3.f, 2.f,
               [](const Prop& p) { return (((int)p.x >> 4) & 1) != 0; }),
        pStanding(P_PALM, SPR_P(c.A.B.palm), 0.95f, 1.f, 0.f),  // palms sway a little; the shadow stands still, which at 1 degree nobody sees
        pStanding(P_WITCHTREE, SPR_P(c.A.B.witchtree), 0.9f, 1.f, -1.f),

        // rocks: cast from the whole bottom edge. Which picture (tiny / small / large, which look) depends on the prop.
        pCols(P_ROCK0, SPR_P(c.A.rockTiny[0][rockLookOf(c.pr)]), 0.9f, 0.9f, -1.f),
        pCols(P_SANDROCK0, SPR_P(c.A.rockTiny[ROCK_THEME_DESERT][rockLookOf(c.pr)]), 0.9f, 0.9f, -1.f),
        pCols(P_SNOWROCK, SPR_P(c.A.rockTiny[0][rockLookOf(c.pr)]), 0.9f, 0.9f, -1.f),
        pCols(P_ROCK1, SPR_P(c.A.rockSmall[0][rockLookOf(c.pr)]), 0.9f, 0.9f, -1.f),
        pCols(P_SANDROCK1, SPR_P(c.A.rockSmall[ROCK_THEME_DESERT][rockLookOf(c.pr)]), 0.9f, 0.9f, -1.f),
        pCols(P_BOULDER, SPR_P(c.A.rockLarge[0][rockLookOf(c.pr)]), 0.9f, 0.9f, -1.f),
        pCols(P_SANDBOULDER, SPR_P(c.A.rockLarge[ROCK_THEME_DESERT][rockLookOf(c.pr)]), 0.9f, 0.9f, -1.f),

        // town buildings: cast column by column from the bottom edge
        pCols(P_BLD0, SPR_P(c.A.T.bld[0]), 0.8f, 0.7f, -1.f), pCols(P_BLD1, SPR_P(c.A.T.bld[1]), 0.8f, 0.7f, -1.f),
        pCols(P_BLD2, SPR_P(c.A.T.bld[2]), 0.8f, 0.7f, -1.f), pCols(P_BLD3, SPR_P(c.A.T.bld[3]), 0.8f, 0.7f, -1.f),
        pCols(P_BLD4, SPR_P(c.A.T.bld[4]), 0.8f, 0.7f, -1.f), pCols(P_BLD5, SPR_P(c.A.T.bld[5]), 0.8f, 0.7f, -1.f),

        // huts, ruins and the things people leave standing about
        pWide(P_HUT, SPR_P(c.A.hut)),
        pWide(P_STILTHUT, SPR_P(c.A.B.stilthut)),
        pWide(P_ICEHUT, SPR_P(c.A.B.icehut)),
        pWide(P_RUINWALL, SPR_P(c.A.B.ruinwall)),
        pWide(P_ARCH, SPR_P(c.A.B.arch)),
        pWide(P_HMARKET, SPR_P(c.A.H.market)),
        pWide(P_LOG, SPR_P(c.A.B.log), 0.7f, 0.9f),
        pWide(P_BENCH, SPR_P(c.A.T.bench), 0.7f, 0.9f),
        pWide(P_WELL, SPR_P(c.A.T.well), 0.8f, 0.8f),
        pStanding(P_CACTUS0, SPR_P(c.A.B.cactus[0])),
        pStanding(P_CACTUS1, SPR_P(c.A.B.cactus[1])),
        pStanding(P_COLUMN, SPR_P(c.A.B.column)),
        pStanding(P_STANDING, SPR_P(c.A.B.standing)),
        pStanding(P_CAIRN, SPR_P(c.A.B.cairn), 0.85f),
        pStanding(P_GRAVE, SPR_P(c.A.B.grave), 0.85f),
        pStanding(P_CRATE, SPR_P(c.A.crate), 0.8f),
        pStanding(P_BARREL, SPR_P(c.A.barrel), 0.8f),
        pStanding(P_SIGN, SPR_P(c.A.sign), 0.7f),
        pStanding(P_LAMP, SPR_P(c.A.T.lamp), 0.85f),
        // ---> add new props here
    };
    return T;
}

// ---- placed structures (chests...). The campfire has no line: it casts nothing.
inline const std::vector<StructShadow>& structShadowTable() {
    static const std::vector<StructShadow> T = {
        sStanding(PL_CHEST, SPR_S(c.A.R.chest), 0.8f),
        // ---> add new placeables here
    };
    return T;
}

// ---- homestead pieces
inline const std::vector<HomeShadow>& homeShadowTable() {
    static const std::vector<HomeShadow> T = {
        // walls: the picture depends on the wall above (a wall that continues upwards is drawn taller)
        hStanding(HP_WALL_WOOD,
                  SPR_H(c.A.H.wall[0][([&] { const HPiece* up = c.g.hsAt(c.pc.tx, c.pc.ty - 1, HL_OBJECT); return up && up->kind == c.pc.kind ? 1 : 0; })()]),
                  {8, 0, 16, 0}, 0.85f),
        hStanding(HP_WALL_STONE,
                  SPR_H(c.A.H.wall[1][([&] { const HPiece* up = c.g.hsAt(c.pc.tx, c.pc.ty - 1, HL_OBJECT); return up && up->kind == c.pc.kind ? 1 : 0; })()]),
                  {8, 0, 16, 0}, 0.85f),
        // buildings: base line along the bottom, centred. rot&1 picks the turned picture
        hWide(HP_SHED, SPR_H(c.A.H.building[HP_SHED][c.pc.rot & 1]), {0, 0.5f, 0, 1.f}),
        hWide(HP_HOUSE, SPR_H(c.A.H.building[HP_HOUSE][c.pc.rot & 1]), {0, 0.5f, 0, 1.f}),
        hWide(HP_BARN, SPR_H(c.A.H.building[HP_BARN][c.pc.rot & 1]), {0, 0.5f, 0, 1.f}),
        hWide(HP_COOP, SPR_H(c.A.H.building[HP_COOP][c.pc.rot & 1]), {0, 0.5f, 0, 1.f}),
        hWide(HP_WORKSHOP, SPR_H(c.A.H.building[HP_WORKSHOP][c.pc.rot & 1]), {0, 0.5f, 0, 1.f}),
        hWide(HP_WAREHOUSE, SPR_H(c.A.H.building[HP_WAREHOUSE][c.pc.rot & 1]), {0, 0.5f, 0, 1.f}),
        hWide(HP_GREENHOUSE, SPR_H(c.A.H.greenhouse[c.pc.rot & 1]), {1, 0, 0, 1.f}, 0.55f),  // glass: a pale shadow
        hWide(HP_WELL, SPR_H(c.A.H.well), {16, 0, 0, 1.f}, 0.8f, 0.8f),
        hWide(HP_BENCH, SPR_H(c.A.T.bench), {0, 0.5f, 14, 0}, 0.7f, 0.9f),
        hStanding(HP_BARREL, SPR_H(c.A.barrel), {8, 0, 14, 0}, 0.8f),
        hStanding(HP_SCARECROW, SPR_H(c.A.H.scarecrow), {8, 0, 15, 0}, 0.8f),
        hStanding(HP_LAMP, SPR_H(c.A.T.lamp), {8, 0, 14, 0}, 0.85f),
        // ---> add new homestead pieces here
    };
    return T;
}

// ---- lookups by kind (built once)
template <class Def>
inline std::vector<const Def*> indexByKind(const std::vector<Def>& T) {
    int mx = 0;
    for (const Def& d : T) mx = std::max(mx, d.kind);
    std::vector<const Def*> v((size_t)mx + 1, nullptr);
    for (const Def& d : T) v[(size_t)d.kind] = &d;
    return v;
}
inline const PropShadow* propShadow(int kind) {
    static const std::vector<const PropShadow*> I = indexByKind(propShadowTable());
    return kind >= 0 && (size_t)kind < I.size() ? I[(size_t)kind] : nullptr;
}
inline const StructShadow* structShadow(int kind) {
    static const std::vector<const StructShadow*> I = indexByKind(structShadowTable());
    return kind >= 0 && (size_t)kind < I.size() ? I[(size_t)kind] : nullptr;
}
inline const HomeShadow* homeShadow(int kind) {
    static const std::vector<const HomeShadow*> I = indexByKind(homeShadowTable());
    return kind >= 0 && (size_t)kind < I.size() ? I[(size_t)kind] : nullptr;
}

}  // namespace shadowdef
