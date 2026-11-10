// hunter_chibi.hpp - the hunter drawn as a big-headed chibi (lime hair, brown tunic, green backpack), built from a 12 x 16 template per view.
// Template letters: a black, b hair shade / green strap, c hair, d skin, e skin shade, f skin light, g dark green, h dark tunic, i tunic.
#pragma once
#include "player/appearance.hpp"
#include "core/gfx.hpp"
#include "player/hunter_pose.hpp"

namespace chibi {
static const char* const FRONT[16] = {
    "...aaaaaa...",
    "..abccccba..",
    ".abccccccba.",
    ".acccccccca.",
    ".adddddddda.",
    "aeddaddaddea",
    "aeddaeeaddea",
    ".adddddddda.",
    "..adddddda..",
    "..gahhhhag..",
    ".gagiiiigag.",
    ".adbiiiibda.",
    "addbiiiibdda",
    "addhiiiihdda",
    ".aahhaahhaa.",
    "..addaadda..",
};
static const char* const BACK[16] = {
    "...aaaaaa...",
    "..abccccba..",
    ".abccccccba.",
    ".acccccccca.",
    ".acccccccca.",
    "aeccccccccea",
    "aeddccccddea",
    ".aeddddddea.",
    "..aeeeeeea..",
    "...gggggg...",
    "..agbbbbga..",
    ".adgbggbgda.",
    "addgbbbbgdda",
    "addggggggdda",
    ".aahhaahhaa.",
    "..addaadda..",
};
static const char* const SIDE_R[16] = {
    "...aaaaa....",
    "..abcccba...",
    ".abcccccba..",
    ".accccccca..",
    ".aceccccca..",
    ".aefdddada..",
    ".aefdddadda.",
    ".aeedddddda.",
    "..aeedddda..",
    ".ggaaghaa...",
    ".ggadbiiaa..",
    ".gaddbiiaea.",
    ".adddbiiaea.",
    ".addaaaaaa..",
    ".gaahhaha...",
    "...adda.....",
};

// One drawn pixel of a template: colour plus whether it is an eye (so it can blink and turn)
struct Cell {
    u32 c = 0;
    bool eye = false;
};

inline u32 colourOf(char ch, int row, const HunterLook& L, bool side) {
    (void)side;
    const OutfitCol& oc = OUTFITCOL[L.outfit];
    const u32 hair = HAIRCOL[L.color], skin = SKINCOL[L.skin], skinS = SKINSHADE[L.skin];
    switch (ch) {
        case 'a': return HEX(0x000000);
        case 'b': return row < 9 ? shadeC(hair, 0.72f) : HEX(0x2fa007);
        case 'c': return hair;
        case 'd': return skin;
        case 'e': return skinS;
        case 'f': return mixc(skin, skinS, 0.45f);
        case 'g': return HEX(0x195205);
        case 'h': return oc.dark;
        case 'i': return oc.base;
    }
    return 0;
}
}  // namespace chibi

// The hunter in one view (dir: 0 front, 1 back, 2 side facing right, 3 front three-quarter, 4 back three-quarter) and pose.
// Same canvas and anchor as the old hunter: 18 x 26, feet end on row 23, the ground point is (9, 24).
inline Img mkChibiPose(int dir, const HPose& P, const HunterLook& look) {
    using namespace chibi;
    Img im(18, 26);
    const HunterLook L = clampLook(look);
    const bool side = dir == 2, back = dir == 1;
    const char* const* T = side ? SIDE_R : back ? BACK : FRONT;
    const int OX = 3, OY = 8;  // template origin on the canvas
    // ---- colour grid; the template's own black contour is dropped (it is redrawn around the finished shape)
    Cell g[16][12];
    auto tc = [&](int x, int y) { return x < 0 || y < 0 || x >= 12 || y >= 16 ? '.' : T[y][x]; };
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 12; x++) {
            const char ch = T[y][x];
            if (ch == '.') continue;
            if (ch == 'a' && (tc(x - 1, y) == '.' || tc(x + 1, y) == '.' || tc(x, y - 1) == '.' || tc(x, y + 1) == '.')) continue;
            g[y][x].c = colourOf(ch, y, L, side);
            if (y >= 5 && y <= 6 && ch == 'a' && ((!side && !back && (x == 4 || x == 7)) || (side && x == 7))) g[y][x].eye = true;
        }
    const u32 skin = SKINCOL[L.skin];
    const int bob = std::max(0, std::min(2, P.bob));
    // blinking: the upper eye pixel closes
    if (P.blink)
        for (int x = 0; x < 12; x++)
            if (g[5][x].eye) g[5][x] = Cell{skin, false};
    auto put = [&](int x, int y, u32 c) {
        if (c) im.set(OX + x, OY + y, c);
    };
    // ---- feet (rows 14-15), drawn first so a raised foot tucks under the tunic
    int dxA = 0, dxB = 0, liftA = 0, liftB = 0;
    if (side) {
        const int d = std::max(-1, std::min(1, (P.sb - P.sa) / 3));
        dxA = -d; dxB = d;
        liftA = std::min(1, P.slA); liftB = std::min(1, P.slB);
    } else {
        liftA = std::min(1, P.liftL); liftB = std::min(1, P.liftR);
    }
    for (int y = 14; y < 16; y++)
        for (int x = 0; x < 12; x++) {
            if (!g[y][x].c) continue;
            const bool a = side ? x < 5 : x < 6;
            const bool moves = !side || y == 15;  // in the side view only the toes slide, the shin stays under the tunic
            put(x + (moves && side ? (a ? dxA : dxB) : 0), y - (moves ? (a ? liftA : liftB) : 0), g[y][x].c);
        }
    // ---- the body (rows 0-13) rises by the bob; the gap under it is filled with a tunic row
    for (int k = 1; k <= bob; k++)
        for (int x = 0; x < 12; x++) put(x, 13 - bob + k, g[12][x].c);
    for (int y = 0; y < 14; y++)
        for (int x = 0; x < 12; x++) {
            Cell c = g[y][x];
            if (!c.c) continue;
            int px = x, py = y - bob;
            if (!side && y >= 9 && (x <= 2 || x >= 9)) py += (x <= 2 ? P.aL : P.aR) * 1;  // the arms swing
            put(px, py, c.c);
        }
    // ---- hairstyles beyond the short bowl cut
    {
        const u32 hair = HAIRCOL[L.color], hd = shadeC(hair, 0.72f), hl = shadeC(hair, 1.15f), tie = HEX(0xe8584f);
        const int hx = 0, hy = -bob;
        auto hp = [&](int x, int y, u32 c) { put(x + hx, y + hy, c); };
        switch (L.hair) {
            case 1:  // long
                if (!side && !back) {
                    for (int y = 5; y <= 11; y++) { hp(0, y, y & 1 ? hd : hair); hp(11, y, y & 1 ? hair : hd); }
                } else if (back) {
                    for (int y = 8; y <= 12; y++) for (int x = 2; x <= 9; x++) hp(x, y, x == 5 || x == 6 ? hd : hair);
                } else {
                    for (int y = 4; y <= 11; y++) for (int x = 0; x <= 2; x++) hp(x, y, x == 0 ? hd : hair);
                }
                break;
            case 2:  // ponytail
                if (!side && !back) { hp(10, 2, tie); hp(11, 3, hair); hp(11, 4, hair); hp(11, 5, hd); }
                else if (back) { hp(5, 8, tie); for (int y = 9; y <= 12; y++) { hp(5, y, hair); hp(6, y, hd); } }
                else { hp(0, 2, tie); for (int y = 3; y <= 7; y++) { hp(-1, y, hair); hp(0, y, hd); } }
                break;
            case 3:  // two braids
                for (int y = 7; y <= 12; y++) {
                    const u32 c = (y & 1) ? hair : hd;
                    if (side) hp(0, y, c);
                    else { hp(0, y, c); hp(11, y, c); }
                }
                if (side) hp(0, 13, tie); else { hp(0, 13, tie); hp(11, 13, tie); }
                break;
            case 4:  // a bun on top
                hp(5, -1, hair); hp(6, -1, hair); hp(4, -2, hair); hp(5, -2, hl); hp(6, -2, hair); hp(7, -2, hair);
                hp(5, -3, hair); hp(6, -3, hair);
                break;
            default: break;
        }
    }
    im.outline(OUT);
    return im;
}
