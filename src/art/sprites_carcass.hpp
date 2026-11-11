// sprites_carcass.hpp - hand-drawn pixel art for the carcass materials (meat, organs, fangs, bones, rabbit foot, claw, leather).
// Each picture is a small palette + rows of characters ('.' is clear), padded to a square canvas because icons are drawn square.
// Generated from the artist's sheet; edit the rows to repaint.
#pragma once
#include "core/gfx.hpp"

struct ArtPix { int w, h; const char* pal; const char* const* rows; };  // pal: "<char><rrggbb>..." entries, 7 chars each
static const char* const ART_MEAT_ROWS[22] = {
    "........aaaaaaaaa.....",
    "......aabbbbbbbbbac...",
    ".....abbbdddddebffbc..",
    "....abfeeghhhhgddbfbc.",
    "...affeeehhhiiheddefc.",
    "..abfedddiieehhgjkgbfc",
    "..afeedgjiiihhhddgkgfc",
    ".alfkkgeegiihhddeeegfc",
    ".afkgeeddddegdddemegnc",
    "aofddgeddeeehemmmmdfnc",
    "afhddhgddmmmhgmmmddnpc",
    "afhdddhdjjmeghddddnnqc",
    "aremjjhjmeedddnnnnpqcs",
    "aredmmhmefndpqtttttcus",
    "areddehennddttccctvuus",
    "afnddhhnftddwwwcctvux.",
    "coddnnnnttdddwwcctxx..",
    "scdfnfqttddddcxxxx....",
    "swcccccccdddxx........",
    "swwyywwccdxd..........",
    ".swwwwwwcxdd..........",
    "..xxxxxxx.............",
};
static const ArtPix ART_MEAT = {22, 22, "a905352bd87782c74382ed9b0d0ded20530fee948cge77268hfdc79bifee6bajff8f71kff8e70ld97889mf45849nffaa9dod57882pd77783qba5e75rffa998s27102ct4a2b0eu796238v55402dwbb7731x000000yc68a4d", ART_MEAT_ROWS};
static const char* const ART_ORGAN_ROWS[22] = {
    "........aaaaaaa.......",
    ".....aaaabbbbaaaa.....",
    ".....acccaaaadbbaaaa..",
    ".....aacccccdbbaaeeea.",
    ".......adbcdbaaaeaaea.",
    "......aaaaaaafeaaaffa.",
    "......aefeefafaaffaa..",
    ".....adaaaaaaaeaaaea..",
    "....adebbdbbbaeaaaaea.",
    "...adedbddgggbaeaa.a..",
    "..adeeddeddggbaeaba...",
    "..aeeeeeeddggbafaba...",
    ".adeeeeedddgggbabbba..",
    ".addeeeddddgcgbbbbba..",
    ".acddeedddgcgggcbbch..",
    ".hcgddddddgcggccccch..",
    ".hccggddcggcgcccccch..",
    ".hhcccgcccccchcccchh..",
    "..hcchhccccchcchhhh...",
    "...hhchhcccccchhhh....",
    "....hhcccchhhhhhh.....",
    "......hhhhhhhhh.......",
};
static const ArtPix ART_ORGAN = {22, 22, "a2a1911b6b3928c9b0d0ddb8693deddb489f977352g9b5031h520303", ART_ORGAN_ROWS};
static const char* const ART_FANG_ROWS[24] = {
    "...........aaaaaaaaa....",
    "..........aabbaabaaaa...",
    ".........aabbbbabbbbba..",
    ".........cbdeebbbfbbba..",
    ".........cbeegbbffffga..",
    ".........cegghhbggfggc..",
    ".........cghhhhhiiggcc..",
    ".........chjjhhhiiigc...",
    ".........chjjhhhiiigc...",
    ".........chjjhhiiiic....",
    ".........chjjhhiiigc....",
    "........chhjhhhiiigc....",
    "........chjjhhiiiba.....",
    "........chjjhhiiiba.....",
    ".......chhjhhhiiic......",
    ".......chjjhhiiaac......",
    "......chjjhhhibaa.......",
    "......chjhhhiiba........",
    ".....cbahhhiiba.........",
    "....chjhhhiigc..........",
    "...cbbbhhiigc...........",
    "..caabhiiicc............",
    ".caabbbgcc..............",
    ".ccccccc................",
};
static const ArtPix ART_FANG = {24, 24, "a520303b9b0d0dc12101edfaead0ef1cfaafd1ad8dgb08d6fhf4e4caid1bb92jfefaee", ART_FANG_ROWS};
static const char* const ART_BONE_ROWS[52] = {
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "...............................aaaa.................",
    "..............................abbbba................",
    ".............................abccbba................",
    "..............aaa............accccbba...............",
    ".............addba...........acceeabaaff............",
    "............affddba..........aceeebaaddff.....ff....",
    "............afaccca..........aceeeceeedfff..affff...",
    "..........aaaaaccca........aacceeeeeeeedffaadddfff..",
    ".........fffceeeecca.....adddccceeeeeddddfbccdddff..",
    ".........ffdeeeeeeecaa.aaddccceeeeeeedddabcccddaff..",
    ".........ffdddeeeeeeccaaddcceecbaaaaaaaaceeeeccaaa..",
    "..........fcccccceeeecccabeecbaabbbccccceeeeeeeecaa.",
    "...........aaaaaaacceeeccaabaabbcddddcceeeeeeeeeeea.",
    "...............aabaacceeeccaabbbbbbfffdddceeeeeeedca",
    "............aaacccbbaaceeeecbaaaaafffffbbcccceeeddca",
    "........aaaaaccccccbbbacceeeccaa.......aaabccceddcca",
    "..aaaaaacccccceccccbaa.aacceeecba.........aaacdccca.",
    ".affcccceeeeceeeebaa.....aaceeecbaa..........aaaaa..",
    "afffddceeeeeeeebaa.........aaceeccbaa...............",
    "ffbccdceeeeecbaa.............aaeeeeecaaaaa..........",
    "fffbabcceeeeba.................aceeeecbddfff........",
    "fffbbabeeeeeba..................aceeeecbdffff.......",
    "..aaaadeeeeca....................aceeeccbdfff.......",
    ".....adeeeeba....................aceeeecddfff.......",
    "....abdeeeeba.....................adeeeaffff........",
    "....addceeca......................adddeaaf..........",
    "....addddcba.......................addca............",
    ".....abbbba........................addda............",
    "......aaaa..........................aaa.............",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
    "....................................................",
};
static const ArtPix ART_BONE = {52, 52, "a12101ebd1bb92cf4e4cad9b0d0defefaeef520303", ART_BONE_ROWS};
static const char* const ART_FOOT_ROWS[20] = {
    "....aaaa............",
    "..aaaabbcc..........",
    ".aabbbbbbac.........",
    "aaabbbdbdddc........",
    "abbbbbddddddc.......",
    "abeebddddddeec......",
    "ceefbbdeeddddec.....",
    "cfefddeedddddddc....",
    ".ceefdfedddddddec...",
    ".cfefffdddddddddc...",
    "..ceeffdddddddddec..",
    "...ceefddddddddddc..",
    "....ceeddddddddddec.",
    ".....cefdddddddeddc.",
    ".....cefedddedddedc.",
    ".....cfefddddeddfdc.",
    "......ceefeddfddfec.",
    ".......cfeeeefeeec..",
    "........ccfeeeecc...",
    "..........ccccc.....",
};
static const ArtPix ART_FOOT = {20, 20, "a520303b9b0d0dc12101edffffffebcb7b7f8a8a8a", ART_FOOT_ROWS};
static const char* const ART_CLAW_ROWS[29] = {
    ".............................",
    "................aabbb........",
    "...........aaaaacccdbbb......",
    "..........aeecceeeccddbb.....",
    ".......aaaaaeeeaaaeeccdbb....",
    "......afggghaeaeccccccdbbb...",
    "....aaggggghaaeeeccccebbbbb..",
    "...afggghhhhaaaaeecceeeeddb..",
    "..afgghhhhhahhhhaeeeaaeccddb.",
    "..agghhhhhaffffhhaeaeccccddb.",
    ".afghhhaaafgffhhhaaaaecccdbbb",
    "afghhaa.afggfhhhhaahhaecccbbb",
    "affha...aggfhhhhaafffhaeeeebb",
    "afha...afgfhhhhaafgfhhaeeebbb",
    "afha...agghhhaaafgghhhaeeeeb.",
    "afa...afgghha..agghhhhaeeeeb.",
    "afa...afghha..afgghhhaeeeea..",
    ".aa...afgha...afghhhaaaaaa...",
    "......afgha...affhha.........",
    "......affa....afhha..........",
    "......affa....afhha..........",
    ".......afa....afha...........",
    ".......afa....afha...........",
    "........aa....afha...........",
    "...............aha...........",
    "...............aha...........",
    "................aa...........",
    ".............................",
    ".............................",
};
static const ArtPix ART_CLAW = {29, 29, "a12101eb520303c885a43d9b0d0de51302bff4e4cagfefaeehd1bb92", ART_CLAW_ROWS};
static const char* const ART_LEATHER_ROWS[28] = {
    ".........aaaaaaaaaa.........",
    "...aa...aabcccccccaa...aa...",
    "..abbaaaabcddddddcbaaaabba..",
    ".aaccbbbccddddddddcbbbbccaa.",
    ".acddcccdddddddddddcccceeca.",
    "afddddddddddddddddddeeeeeefa",
    "abcdddddddddddddddddeeeeecba",
    "afbcddddddddddddddddeeeecbfa",
    ".afbcdddddddddddddddeeecbfa.",
    "..afbcdddddddddddddeeecbfa..",
    "...abcdddddddddddddeeecba...",
    "....abcdddddddddddeeecba....",
    "....abcddddddddddeeeecba....",
    "....abcdddddddeeeeeeecba....",
    "....abcdddddddeeeeeeecba....",
    "....abcddddddeeeeeeeecba....",
    "...abcddddddeeeeeeeeeecba...",
    "..abcddddddeeeeeeeeeeeecba..",
    ".abcddddddeeeeeeeeeeeeeecba.",
    "abcddddddeeeeeeeeeeeeeeeecba",
    "abcdddddeeeeeeeeeeeeeeeeecba",
    "afbcddcccceeeeeeeecccceecbfa",
    ".afbccbbbbcceeeeccbbbbccbfa.",
    "..abbbaaabbbceecbbbaaabbba..",
    "...aaa...aabbccbbaa...aaa...",
    "..........aafbbfaa..........",
    "............affa............",
    ".............aa.............",
};
static const ArtPix ART_LEATHER = {28, 28, "a12101eb74432dcbc7b4fd904d34e9f5c3af2e1e1d", ART_LEATHER_ROWS};

inline Img artToImg(const ArtPix& a) {
    Img im(a.w, a.h);
    for (int y = 0; y < a.h; y++)
        for (int x = 0; x < a.w; x++) {
            const char c = a.rows[y][x];
            if (c == '.') continue;
            for (const char* p = a.pal; *p; p += 7)
                if (*p == c) {
                    im.set(x, y, 0xFF000000u | (uint32_t)strtoul(std::string(p + 1, 6).c_str(), nullptr, 16));
                    break;
                }
        }
    return im;
}

// the material with hand-drawn art, or nullptr (then the procedural icon is used)
inline const ArtPix* carcassArt(int m) {
    switch (m) {
        case M_MEAT: return &ART_MEAT;
        case M_ORGAN: return &ART_ORGAN;
        case M_BONE: return &ART_BONE;
        case M_CLAW: return &ART_CLAW;
        case M_LEATHER: return &ART_LEATHER;
        case M_RABBITFOOT: return &ART_FOOT;
        case M_WOLFFANG: case M_COYOTEFANG: case M_COUGARFANG: case M_SPIDERFANG: case M_FROSTFANG: return &ART_FANG;
        default: return nullptr;
    }
}
