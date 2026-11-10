// ref_art_boar32.hpp - the boar as 32x32 pixel art: the side view (facing right) is the supplied sprite, the front / back views are drawn in its palette.
// Letters: a body, b darker edge, d shade, m far leg, n far hoof, k dark mane / hoof / nose, t snout, w tusk, x pale jaw, y spare.
#pragma once
namespace deerref {
static const char* const BOAR32_SIDE[] = {
    ".............k.............",
    "..............kk..k........",
    "............kkkkkk.k.......",
    "..........kkkkkkkkkk.k.....",
    ".........kkbbbbbbkkkk.k....",
    ".......kkbbaaaabbbbkkkk....",
    ".....kbbaaaaaaaaaabbkkk....",
    ".....aaaaaaaaaaaaaabkkk....",
    "kk.kbaaaaaaaaaaaaaaabkk....",
    ".kk.aaabaaabdaaaaaaabbb....",
    "...baabdbbbdbaaadakkdddk...",
    "...baaabdbbdbbaabdaaattt..w",
    "...baaabddddbbaabdxaaywyww.",
    "...baabbmmmmmbbabdxwwwttx..",
    "....bbbmm....bbbb.dxxdd....",
    "....bb.nn......bb.nn.......",
    "....kk.........kk.........."
};
static const char* const BOAR32_FRONT[] = {
    ".......kk.......",
    "......kkkk......",
    "..aa.kkkkkk.aa..",
    ".addaaakkaaadda.",
    "..aaaaakkaaaaa..",
    ".aaakaaaaaakaaa.",
    ".aaaaaaaaaaaaaa.",
    ".aaaattttttaaaa.",
    ".aaattkttkttaaa.",
    "..awttttttttwa..",
    "...aattttttaa...",
    "...bbbbbbbbbb...",
    "..bbbbbbbbbbbb..",
    "...aa......aa...",
    "...aa......aa...",
    "...aa......aa...",
    "...kk......kk..."
};
static const char* const BOAR32_BACK[] = {
    "................",
    ".......kk.......",
    "..aa..kkkk..aa..",
    ".addaakkkkaadda.",
    "..aaaakkkkaaaa..",
    ".aaaaaakkaaaaaa.",
    ".aaaaaakkaaaaaa.",
    ".aaaaaakkaaaaaa.",
    ".aaaaaaaaaaaaaa.",
    "..aaaaaddaaaaa..",
    "...aaaaddaaaa...",
    "...bbbbbbbbbb...",
    "..bbbbbbbbbbbb..",
    "...aa......aa...",
    "...aa......aa...",
    "...aa......aa...",
    "...kk......kk..."
};
constexpr int BOAR32_W = 27, BOAR32_H = 17, BOAR32_FW = 16, BOAR32_FH = 17;
}  // namespace deerref
