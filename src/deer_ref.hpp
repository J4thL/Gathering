// deer_ref.hpp - the doe and stag drawn from the pixel-art reference, pixel for pixel (east view), animated by moving parts of that art.
//
// The reference is one flat, outline-free side view (doe 46x46, stag 46x56, the stag was drawn facing left and is mirrored here). Every
// frame is made from those pixels: the legs are sheared and lifted around their hips, the head and neck turn (and the neck stretches
// to graze), the tail flicks, the body bobs, lies down or rolls. The three east-ish views (SE, E, NE) come from this art; the front
// and back views (S, N) are still drawn by the 3D rig in sprites_rig.hpp, in the same palette. Colours: letters below are
// a/b browns, m maroon far legs, d shade, c cream, t tan, k near-black, p/q pink ear.
#pragma once
#include <cmath>
#include <string>
#include <vector>

namespace deerref {

static const char* const DOE_ROWS[] = {
    "..............................aaa.............",
    "..............................cppa...dd.......",
    "..............................cppba.ddd.......",
    "..............................ccqqbaaad.......",
    "...............................cqqbbaaaa......",
    ".................................abbbbaaaa....",
    ".................................abbbbkbaaaa..",
    ".................................abbbbkbbbbakk",
    ".................................abbbbbbbbbbbk",
    ".................................abbbbbbbbbbbb",
    ".................................abbbbbdddddd.",
    ".................................abbbbdt......",
    "................................abbbbbt.......",
    "................................abbbbbt.......",
    "...............................abbbbbbt.......",
    "..........aaaaaaaaaaaaaaaaaaaaaabbbbbbt.......",
    "a.......aaaaaaaaaaaaaaaaaaaaaaaaabbbbbt.......",
    "caa...aaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbd.......",
    ".ccaaaccbaaaaaaaaaaaaaaaaaaaaaaaaaabbbd.......",
    "..ccccttbaaaaaaaabaaaaaaaaaaaaaaaaabbbd.......",
    "....ttttbaaaaaaabbbaaaaaaaaaaaaaaaabbbd.......",
    ".....ttttaaaaaaaabdbaaaaaaaaaaaaaaabbb........",
    "........taaaaaaaabdbbaaaaaabbaaaaabbbb........",
    "........tbbaaaaaabdbbbaaaaddaaaaabbbbd........",
    "........tbbaaaaaabddbbbbbbbdaaaabbbbbd........",
    ".........tbbaaaaabdddbbbbbdaaaabbbbbd.........",
    "..........bbaaaabbddddbbbddaabbbbbbdd.........",
    "..........aaaaabbddddddddddaabbbbbdd..........",
    ".........aaaaabbbdmtttttddaaabbbbdd...........",
    "........aaabbbbbdmmmm...ttaaabbddmm...........",
    ".......aabbbbbbdmmmm.......abbbmmmm...........",
    ".......abbbbbddmmmm........abbbmmmm...........",
    ".......abbbddmmmm..........abbbmmmm...........",
    ".......abbdmmm.............abbbmmmm...........",
    ".......abb.mmm.............abbbmmmm...........",
    ".......abb.mmm..............abb.mmm...........",
    ".......abb.mmm..............abb.mmm...........",
    ".......abb.mmm..............abb.mmm...........",
    ".......abb.mmm..............abb.mmm...........",
    ".......abb.mmm..............abb.mmm...........",
    ".......abb.mmm..............abb.mmm...........",
    ".......abb.mmm..............abb.mmm...........",
    ".......abb.mmm..............abb.mmm...........",
    ".......abb.mmm..............bbb.mmm...........",
    "........bb..mm...............bb..mm...........",
    "........kk..kk...............k...k............"
};
constexpr int DOE_W = 46, DOE_H = 46;

// the stag as drawn (facing left)
static const char* const STAG_ROWS[] = {
    ".............c................................",
    ".......t......c...............................",
    "........t.c...c...............................",
    "......c.t.c....c..c...........................",
    ".....t.c.t.ct..c..c...........................",
    "..t..t..cccccccc.c............................",
    "...t..t..t.t..ccc.............................",
    "....ttttttt..aaa..............................",
    ".......ddttcappc..............................",
    ".......dtdcabppc..............................",
    ".......daaabqqpc..............................",
    "......aaaabbqqc...............................",
    "....aaaabbbba.................................",
    "..aaaabkbbbba.................................",
    "kkabbbbkbbbba.................................",
    "kbbbbbbbbbbba.................................",
    "bbbbbbbbbbbba.................................",
    ".dddddbbbbbbaa................................",
    ".....tdbbbbbaa................................",
    "......tbbbbbba................................",
    "......tbbbbbbaa...............................",
    "......tbbbbbbbaa..............................",
    "......tbbbbbbbaaaaaaaaaaaaaaaaaaaa............",
    "......tbbbbbbaaaaaaaaaaaaaaaaaaaaaaa..........",
    "......tbbbbbaaaaaaaaaaaaaaaaaaaaaaaaaa.......a",
    "......dbbbbaaaaaaaaaaaaaaaaaaaaaaaaaaaa.....aa",
    "......dbbbbaaaaaaaaaaaaaaaaaaaaaaaaaaaaa..aac.",
    "......ddbbbaaaaaaaaaaaaaaaabbbaaaaaaabccaacc..",
    ".......dbbbaaaaaaaaaaaaaaabbbaaaaaaaabttccc...",
    ".......dbbbbaaaaabbaaaaaabbdbaaaaaaabbttt.....",
    ".......dbbbbaaaaaadaaaaabbbdbaaaaaabbbt.......",
    ".......dbbbbbaaaaadbbbbbbbddbaaaaaabbt........",
    "........dbbbbbaaaaadbbbbbdddbaaaaabbbt........",
    "........dbbbbbbbbaaddbbbddddbaaaaabbt.........",
    ".........dbbbbbbbaadddddddddbaaaaaab..........",
    ".........mddbbbbbaadddttttddbbaaaaaa..........",
    "..........mmdddbbaaatt..mmmmdbbaaaaaa.........",
    "...........mmmmbbba.....mmmmdbbbaaaaaa........",
    "...........mmmmbbba......mmmmdbbbbaaaaa.......",
    "...........mmmmbbba.......mmmmdbbbbbbaa.......",
    "...........mmmmbbba........mmmmddbbbbba.......",
    "...........mmmmbbba..........mmmmddbbba.......",
    "...........mmmmbbba............mmmmbbba.......",
    "...........mmmmbbba............mmmm.bba.......",
    "...........mmm.bba.............mmmm.bba.......",
    "...........mmm.bba.............mmmm.bba.......",
    "...........mmm.bba.............mmmm.bba.......",
    "...........mmm.bba.............mmmm.bba.......",
    "...........mmm.bba.............mmmm.bba.......",
    "...........mmm.bba.............mmmm.bba.......",
    "...........mmm.bba.............mmmm.bba.......",
    "...........mmm.bba.............mmmm.bba.......",
    "...........mmm.bba.............mmmm.bba.......",
    "...........mm..bb..............mmm..bb........",
    "...........kk..kk..............kkk..kk........",
    ".............................................."
};
constexpr int STAG_W = 46, STAG_H = 56;

// hand-drawn front (facing the camera) and back views, same palette; 18 wide
static const char* const DOE_FRONT[] = {
    "..................",
    ".aa............aa.",
    ".apa..........apa.",
    ".apa..........apa.",
    "..aa..........aa..",
    "..aab........baa..",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    "....abkbbbbkba....",
    "....abbbbbbbba....",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    "......abbbba......",
    "......akkkka......",
    "......abbbba......",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    ".....abbccbba.....",
    ".....abbccbba.....",
    ".....abbccbba.....",
    "....abbbbbbbba....",
    "...aabbbbbbbbaa...",
    "..aabbbbbbbbbbaa..",
    "..abbbbbbbbbbbba..",
    "..abbbbbbbbbbbba..",
    "..abbbbbbbbbbbba..",
    "..abbbbbbbbbbbba..",
    "..abbbbbbbbbbbba..",
    "..abbbbbbbbbbbba..",
    "..aabbbbbbbbbbaa..",
    "...abbbmmmmbbba...",
    "...mabbmmmmbbam...",
    "...mabb.mm.bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...kkkk....kkkk..."
};
static const char* const DOE_BACK[] = {
    "..................",
    ".aa............aa.",
    ".aba..........aba.",
    ".aba..........aba.",
    "..aa..........aa..",
    "..aab........baa..",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    "......abbbba......",
    "......abbbba......",
    "......abbbba......",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    "....abbbbbbbba....",
    "...aabbbbbbbbaa...",
    "..aabbbbbbbbbbaa..",
    "..abbbbbbbbbbbba..",
    "..abbbbbbbbbbbba..",
    "..abbbbbbbbbbbba..",
    "..abbbbbccbbbbba..",
    "..abbbbbccbbbbba..",
    "..abbbbbccbbbbba..",
    "..aabbbbccbbbbaa..",
    "...abbbmccmbbba...",
    "...mabbmttmbbam...",
    "...mabb.mm.bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...kkkk....kkkk..."
};
static const char* const STAG_FRONT[] = {
    "t...t........t...t",
    "t..tt........tt..t",
    "tt.t..........t.tt",
    ".ttt..........ttt.",
    ".tt..t......t..tt.",
    "..tt.t......t.tt..",
    "..tttt......tttt..",
    "...ttt......ttt...",
    "....tt......tt....",
    ".....tt....tt.....",
    "..................",
    ".aa............aa.",
    ".apa..........apa.",
    ".apa..........apa.",
    "..aa..........aa..",
    "..aab........baa..",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    "....abkbbbbkba....",
    "....abbbbbbbba....",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    ".....abkkkkba.....",
    ".....abbbbbba.....",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    "....abbbccbbba....",
    "....abbbccbbba....",
    "....abbbccbbba....",
    "...abbbbbbbbbba...",
    "..ababbbbbbbbaba..",
    ".ababbbbbbbbbbaba.",
    ".abbbbbbbbbbbbbba.",
    ".abbbbbbbbbbbbbba.",
    ".abbbbbbbbbbbbbba.",
    ".abbbbbbbbbbbbbba.",
    ".abbbbbbbbbbbbbba.",
    ".abbbbbbbbbbbbbba.",
    ".ababbbbbbbbbbaba.",
    "...abbbmmmmbbba...",
    "...mabbmmmmbbam...",
    "...mabb.mm.bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...kkkk....kkkk..."
};
static const char* const STAG_BACK[] = {
    "t...t........t...t",
    "t..tt........tt..t",
    "tt.t..........t.tt",
    ".ttt..........ttt.",
    ".tt..t......t..tt.",
    "..tt.t......t.tt..",
    "..tttt......tttt..",
    "...ttt......ttt...",
    "....tt......tt....",
    ".....tt....tt.....",
    "..................",
    ".aa............aa.",
    ".aba..........aba.",
    ".aba..........aba.",
    "..aa..........aa..",
    "..aab........baa..",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    ".....abbbbbba.....",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    "....abbbbbbbba....",
    "...abbbbbbbbbba...",
    "..ababbbbbbbbaba..",
    ".ababbbbbbbbbbaba.",
    ".abbbbbbbbbbbbbba.",
    ".abbbbbbbbbbbbbba.",
    ".abbbbbbbbbbbbbba.",
    ".abbbbbbccbbbbbba.",
    ".abbbbbbccbbbbbba.",
    ".abbbbbbccbbbbbba.",
    ".ababbbbccbbbbaba.",
    "...abbbmccmbbba...",
    "...mabbmttmbbam...",
    "...mabb.mm.bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...mabb....bbam...",
    "...kkkk....kkkk..."
};
constexpr int FB_W = 18, DOE_FB_H = 46, STAG_FB_H = 56;

constexpr int PAD = 16;  // working margin around the art (rotations and swings need room)

struct Cell { int x, y; char c; };

// Parts of one animal, cut out of the art once.
struct Cut {
    int w = 0, h = 0;
    std::vector<char> g;  // the whole art, facing right
    int kind = 0;  // 0 side (east), 1 front (south), 2 back (north)
    int legTop = 0, headRow = 0, headX0 = 0, tailX1 = 0, tailY0 = 0, tailY1 = 0;
    float headPx = 0, headPy = 0, tailPx = 0, tailPy = 0, neckLen = 10.f;
    char at(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h) ? '.' : g[(size_t)y * w + x]; }
};

inline Cut makeCut(bool stag, int kind) {
    Cut c;
    c.kind = kind;
    if (kind != 0) {  // the hand-drawn front / back views
        const char* const* rows = kind == 1 ? (stag ? STAG_FRONT : DOE_FRONT) : (stag ? STAG_BACK : DOE_BACK);
        c.w = FB_W;
        c.h = stag ? STAG_FB_H : DOE_FB_H;
        c.g.assign((size_t)c.w * c.h, '.');
        for (int y = 0; y < c.h; y++)
            for (int x = 0; x < c.w; x++) c.g[(size_t)y * c.w + x] = rows[y][x];
        const int top = stag ? 10 : 0;  // the stag has ten rows of antlers above the doe's head
        c.legTop = top + 31; c.headRow = top + 14; c.headX0 = 0; c.headPx = 9.f; c.headPy = (float)(top + 15);
        c.tailX1 = -1; c.tailY0 = 0; c.tailY1 = -1; c.tailPx = 0.f; c.tailPy = 0.f;
        return c;
    }
    const char* const* rows = stag ? STAG_ROWS : DOE_ROWS;
    c.w = stag ? STAG_W : DOE_W;
    c.h = stag ? STAG_H : DOE_H;
    c.g.assign((size_t)c.w * c.h, '.');
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) c.g[(size_t)y * c.w + x] = stag ? rows[y][c.w - 1 - x] : rows[y][x];  // the stag is mirrored to face right
    if (stag) {
        c.legTop = 38; c.headRow = 21; c.headX0 = 28; c.headPx = 35.f; c.headPy = 22.f; c.neckLen = 10.f;
        c.tailX1 = 8; c.tailY0 = 22; c.tailY1 = 33; c.tailPx = 9.f; c.tailPy = 28.f;
    } else {
        c.legTop = 34; c.headRow = 21; c.headX0 = 31; c.headPx = 34.f; c.headPy = 22.f; c.neckLen = 10.f;
        c.tailX1 = 7; c.tailY0 = 14; c.tailY1 = 28; c.tailPx = 8.f; c.tailPy = 19.5f;
    }
    return c;
}

// The pixels of `src` turned by `ang` (+ = nose up, counter-clockwise) around (px, py); the part above `neckLen` is kept whole while the
// neck part below it is stretched by `sv`. Inverse mapping, nearest pixel, so a turned part has no holes.
template <class F>
inline void turnPart(const std::vector<Cell>& src, float px, float py, float ang, float neckLen, float sv, F emit) {
    if (src.empty()) return;
    int x0 = 1 << 20, x1 = -(1 << 20), y0 = 1 << 20, y1 = -(1 << 20);
    float rmax = 0;
    for (const Cell& c : src) {
        x0 = std::min(x0, c.x); x1 = std::max(x1, c.x); y0 = std::min(y0, c.y); y1 = std::max(y1, c.y);
        rmax = std::max(rmax, std::hypot(c.x + 0.5f - px, c.y + 0.5f - py));
    }
    const int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
    std::vector<char> grid((size_t)bw * bh, 0);
    for (const Cell& c : src) grid[(size_t)(c.y - y0) * bw + (c.x - x0)] = c.c;
    const float cs = cosf(ang), sn = sinf(ang), R = rmax * std::max(1.f, sv) + 3.f;
    for (int Y = (int)floorf(py - R); Y <= (int)ceilf(py + R); Y++)
        for (int X = (int)floorf(px - R); X <= (int)ceilf(px + R); X++) {
            const float rx = X + 0.5f - px, ru = -(Y + 0.5f - py);
            float x = rx * cs + ru * sn, u = -rx * sn + ru * cs;  // back into the part's own frame
            if (sv != 1.f && u > 0) u = (u <= neckLen * sv) ? u / sv : u - neckLen * (sv - 1.f);
            const int sx = (int)floorf(px + x) - x0, sy = (int)floorf(py - u) - y0;
            if (sx < 0 || sy < 0 || sx >= bw || sy >= bh) continue;
            const char ch = grid[(size_t)sy * bw + sx];
            if (ch) emit(X, Y, ch);
        }
}

inline Img paint(const QuadSpec& sp, const RigShape& R, float tierK, int view, const BPose& P, int CW, int CH, float ax, float ay) {
    static const Cut CUT[2][3] = {{makeCut(false, 0), makeCut(false, 1), makeCut(false, 2)}, {makeCut(true, 0), makeCut(true, 1), makeCut(true, 2)}};
    const Cut& C = CUT[R.antlers ? 1 : 0][view == 0 ? 1 : (view == 4 ? 2 : 0)];
    const bool sideView = C.kind == 0;
    const float u = rigScale(R);  // one rig unit in art pixels
    const int WW = C.w + 2 * PAD, WH = C.h + 2 * PAD;
    std::vector<char> W((size_t)WW * WH, 0);
    auto put = [&](int x, int y, char c) {
        x += PAD; y += PAD;
        if (x >= 0 && y >= 0 && x < WW && y < WH) W[(size_t)y * WW + x] = c;
    };

    // ---- pose -> pixels
    const float lie = std::min(1.f, std::max(P.lie, P.roll / 1.5708f));
    const int bobPx = (int)lroundf((P.bob + P.air * R.airMul) * u + P.breathe * u * 1.5f);
    const int lieDrop = (int)lroundf(lie * (C.h - C.legTop - 1));
    const float pitchDn = std::max(0.f, -P.pitch);
    const int frontDrop = (int)lroundf(pitchDn * 14.f * (1.f - lie));
    const int bodyDx = (int)lroundf(P.bodyDx * u);
    auto bodyDy = [&](int x) {  // how far the body at column x moves down (the front sinks when grazing)
        const float f = std::min(1.f, std::max(0.f, (x - 20.f) / 14.f));
        return lieDrop - bobPx + (int)lroundf(frontDrop * f);
    };

    // legs: group the leg pixels into four by column
    std::vector<char> occ(C.w, 0);
    for (int y = C.legTop + 3; y < C.h; y++)
        for (int x = 0; x < C.w; x++)
            if (C.at(x, y) != '.') occ[x] = 1;
    std::vector<std::pair<int, int>> runs;
    for (int x = 0; x < C.w; x++)
        if (occ[x]) {
            if (!runs.empty() && runs.back().second == x - 1) runs.back().second = x;
            else runs.push_back({x, x});
        }
    auto legOf = [&](int x) {
        int best = 0, bd = 1 << 20;
        for (int i = 0; i < (int)runs.size(); i++) {
            const int d = (x < runs[i].first) ? runs[i].first - x : (x > runs[i].second ? x - runs[i].second : 0);
            if (d < bd) { bd = d; best = i; }
        }
        return best;
    };
    const int legLen = C.h - C.legTop;
    // gait: the near hind and far fore leg move together, the far hind and near fore leg opposite
    float legPhase[8];
    for (int i = 0; i < 8; i++) legPhase[i] = 0;
    std::vector<char> farLeg(runs.size(), 0);
    for (size_t i = 0; i < runs.size(); i++) {
        int m = 0, o = 0;
        for (int y = C.legTop + 3; y < C.h; y++)
            for (int x = runs[i].first; x <= runs[i].second; x++) {
                const char ch = C.at(x, y);
                if (ch == 'm') m++; else if (ch != '.' && ch != 'k') o++;
            }
        farLeg[i] = m > o;
    }
    for (size_t i = 0; i < runs.size() && i < 8; i++) {
        const bool fore = runs[i].first > C.w / 2;
        legPhase[i] = (fore == (farLeg[i] != 0)) ? 0.f : 3.14159f;  // fore far and hind near: 0
    }
    const bool moving = P.stride > 0.01f;
    const float swingA = P.stride * R.strideMul * u * 0.55f, liftA = P.lift * R.liftMul * u * 0.5f;
    const float limp = lie >= 0.5f ? std::max(P.limp, 0.f) : 0.f;

    // ---- cut the art into parts
    std::vector<Cell> head, tail, bodyPx;
    std::vector<std::vector<Cell>> legs(runs.size());
    for (int y = 0; y < C.h; y++)
        for (int x = 0; x < C.w; x++) {
            const char ch = C.at(x, y);
            if (ch == '.') continue;
            if (y >= C.legTop) legs[legOf(x)].push_back({x, y, ch});
            else if (y <= C.headRow && x >= C.headX0) head.push_back({x, y, ch});
            else if (x <= C.tailX1 && y >= C.tailY0 && y <= C.tailY1) tail.push_back({x, y, ch});
            else bodyPx.push_back({x, y, ch});
        }

    // ---- draw: tail, far legs, body, near legs, head
    if (!tail.empty()) {
        const float ang = P.tail * 0.5f + (lie > 0.5f && P.tail > 0 ? 0.2f : 0.f);
        const int dyT = bodyDy((int)C.tailPx);
        std::vector<Cell> t2;
        for (const Cell& c : tail) t2.push_back({c.x, c.y + dyT, c.c});
        turnPart(t2, C.tailPx, C.tailPy + dyT, ang, 0.f, 1.f, [&](int x, int y, char ch) { put(x + bodyDx, y, ch); });
    }
    auto drawLegs = [&](bool far) {
        if (lie > 0.5f && limp <= 0.f) return;  // lying down: the legs are tucked away
        for (size_t i = 0; i < legs.size(); i++) {
            if ((farLeg[i] != 0) != far) continue;
            const float ph = P.phase + legPhase[i < 8 ? i : 0];
            const float sw = (moving && sideView) ? swingA * sinf(ph) : 0.f, lf = moving ? liftA * (sideView ? 1.f : 1.3f) * std::max(0.f, cosf(ph)) : 0.f;
            const float hipX = 0.5f * (runs[i].first + runs[i].second);
            for (const Cell& c : legs[i]) {
                const float t = (c.y - C.legTop) / (float)legLen;  // 0 at the hip, 1 at the hoof
                float fx = c.x + sw * t, fy = c.y - lf * t;
                fy += (bodyDy((int)hipX) ) * (1.f - t);            // the hip follows the body, the hoof stays put
                if (limp > 0.f) {  // death: the leg swings out stiff from the hip
                    const float a = limp * 1.2f, rx = c.x - hipX, ry = c.y - C.legTop;
                    fx = hipX + rx * cosf(a) - ry * sinf(a);
                    fy = C.legTop + rx * sinf(a) + ry * cosf(a) + lieDrop - bobPx;
                }
                put((int)lroundf(fx) + bodyDx, (int)lroundf(fy), c.c);
                put((int)lroundf(fx) + bodyDx, (int)lroundf(fy) + 1, c.c);  // no gaps when the leg is squeezed
            }
        }
    };
    drawLegs(true);
    for (const Cell& c : bodyPx) put(c.x + bodyDx, c.y + bodyDy(c.x), c.c);
    drawLegs(false);
    if (sideView) {
        float ang = (P.pitch < 0.f ? 0.2f : 0.15f) * P.headDz + (P.pitch < 0.f ? 0.6f : 0.35f) * P.pitch;  // grazing (nose down) turns further
        if (std::fabs(ang) < 0.05f) ang = 0.f;
        ang = std::max(-1.7f, std::min(0.6f, ang));
        const float sv = ang < -0.3f ? 1.f + (-ang) * 0.3f : 1.f;  // grazing stretches the neck
        const int dyH = bodyDy((int)C.headPx);
        std::vector<Cell> h2;
        for (const Cell& c : head) h2.push_back({c.x, c.y + dyH, c.c});
        turnPart(h2, C.headPx, C.headPy + dyH, ang, C.neckLen, sv, [&](int x, int y, char ch) { put(x + bodyDx, y, ch); });
    } else {  // seen from the front / back the head nods: it moves down (grazing) or up (alert), the neck follows
        const int dh = std::max(-4, std::min(12, (int)lroundf(-P.headDz * 1.2f) + frontDrop));
        const int dyB = -bobPx + lieDrop;
        for (int y = C.headRow + 1 + std::min(0, dh); y <= C.headRow; y++)
            for (int x = 0; x < C.w; x++) {  // the neck stretches when the head goes up
                const char ch = C.at(x, C.headRow + 1);
                if (ch != '.') put(x + bodyDx, y + dyB, ch);
            }
        for (const Cell& c : head) put(c.x + bodyDx, c.y + dh + dyB, c.c);
    }

    // ---- colours
    const bool exact = sp.body == HEX(0x823e19) && sp.dark == HEX(0x5b131b);
    u32 pal[128];
    for (int i = 0; i < 128; i++) pal[i] = 0;
    if (exact) {
        pal['a'] = HEX(0x823e19); pal['b'] = HEX(0x793819); pal['m'] = HEX(0x5b131b); pal['d'] = HEX(0x682412);
        pal['c'] = HEX(0xd8a284); pal['t'] = HEX(0xba7d5b); pal['k'] = HEX(0x3d0d12);
    } else {  // a rarer tier: the same art in that tier's colours
        pal['a'] = sp.body; pal['b'] = mixc(sp.body, sp.dark, 0.12f); pal['m'] = sp.dark; pal['d'] = mixc(sp.body, sp.dark, 0.55f);
        pal['c'] = sp.belly; pal['t'] = mixc(sp.belly, sp.body, 0.45f); pal['k'] = sp.hoof;
    }
    pal['p'] = HEX(0xd4776c); pal['q'] = HEX(0xb65656);
    if (P.tint > 0.f)
        for (int i = 0; i < 128; i++)
            if (pal[i]) pal[i] = tintC(pal[i], P.tint);

    // ---- to the canvas: the feet on the anchor, scaled by the tier, narrowed for the diagonal views
    Img im(CW, CH);
    const int aix = (int)ax, aiy = (int)ay;
    const float xs = (view == 1 || view == 3) ? 0.8f : 1.f;
    const float cx = PAD + C.w * 0.5f, fy = (float)(PAD + C.h);
    for (int Y = 0; Y < CH; Y++)
        for (int X = 0; X < CW; X++) {
            const float sx = (X + 0.5f - aix) / (tierK * xs) + cx, sy = (Y + 0.5f - aiy) / tierK + fy;
            const int ix = (int)floorf(sx), iy = (int)floorf(sy);
            if (ix < 0 || iy < 0 || ix >= WW || iy >= WH) continue;
            const char ch = W[(size_t)iy * WW + ix];
            if (ch) im.p[(size_t)Y * CW + X] = pal[(int)ch];
        }
    return im;
}

}  // namespace deerref
