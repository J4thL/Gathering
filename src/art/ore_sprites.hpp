// ore_sprites.hpp - the ore rocks (copper, iron, silver, gold): a 30x30 rock whose cracks carry the metal.
// Letters: a-d = the biome's rock stone (outline to light top, taken from ROCK_PALS, so the same rock suits every biome),
// 1-4 = the metal or coal (dark edge, body, light, pale); coal is a pile of black lumps with no stone, and the bog amber deposit has its own brown peat stone (1-4) with the amber in 5-9, so both are the same in every biome, p q = green patina (copper), x y z = rust flecks (iron), w = white glint, t = twinkle (silver).
#pragma once

constexpr int OREM_COPPER = 0, OREM_IRON = 1, OREM_SILVER = 2, OREM_GOLD = 3, OREM_COAL = 4, OREM_AMBER = 5, OREM_N = 6;
constexpr int ORE_W = 30, ORE_H = 30;

static const char* const ORE_ART_COPPER[ORE_H] = {
    "..............aaa.............",
    "............aacccaa...........",
    "............addddca...........",
    "...........addddccba..........",
    "..........addddcccaa..........",
    ".........acddcccca24a.........",
    "........aacccccca142a.........",
    ".......aa1accccba4padaa.......",
    "......ada3abccba11addda.......",
    "......adca2abbba21adddda......",
    ".....adccca2aba43accddda......",
    ".....acccccaaa231acccccaa.....",
    "....accbcdddda131acccbbba.....",
    "....abbbbddddcap3accbbbbba....",
    "...a1abbbbddccba1aacbbbba3a...",
    "...a2abbbbbccbbaa3abbbaa21a...",
    "..a332abbbbcbbbba13aba431ada..",
    "..a22a3abbbbbbbaaap1a2aaadda..",
    "..aaaa3abbbbbbaacda11pacddcba.",
    "...acca2aabbbbadddca1accdccba.",
    "...abcca31abbadddcca3acccccba.",
    "...aabca331aaadcccca32acccbba.",
    ".aa.abbba31aacccccbba3aabbba..",
    "adca.abbb221acccccbba11aabaa..",
    "adba.aabb11abbbcbbbba111aaaaa.",
    ".aa.aa.aapabbbbbbbba11aaa.adba",
    "...adda..aaabbbbbbaaaaa...acba",
    "...acba....aaaaaaaa........aa.",
    "....aa........................",
    "..............................",
};
static const char* const ORE_ART_IRON[ORE_H] = {
    "..............aaa.............",
    "............aacccaa...........",
    "............addddca...........",
    "...........addddccba..........",
    "..........addddcccaa..........",
    ".........acddcccca24a.........",
    "........aacccccca142a.........",
    ".......aa1accccba42adaa.......",
    "......ada3abccba11addda.......",
    "......adca2abbba21adddda......",
    ".....adccca2aba43accddda......",
    ".....acccccaaa231acccccaa.....",
    "....accbcdddda131acccbbba.....",
    "....abbbbddddca13accbbbbba....",
    "...a1abbbbddccba1aacbbbba3a...",
    "...a2abbbbbccbbaa3abbbaa21a...",
    "..a332abbbbcbbbba13aba431ada..",
    "..a22a3abbbbbbbaaa21a2aaadda..",
    "..aaaa3abbbbbbaacda111acddcba.",
    "...acca2aabbbbadddca1accdccba.",
    "...abcca31abbadddcca3acccccba.",
    "...aabca331aaadcccca32acccbba.",
    ".aa.abbba31aacccccbba3aabbba..",
    "adca.abbb221acccccbba11aabaa..",
    "adba.aabb11abbbcbbbba111aaaaa.",
    ".aa.aa.aa1abbbbbbbba11aaa.adba",
    "...adda..aaabbbbbbaaaaa...acba",
    "...acba....aaaaaaaa........aa.",
    "....aa........................",
    "..............................",
};
static const char* const ORE_ART_SILVER[ORE_H] = {
    "..............aaa.............",
    "............aacccaa...........",
    "............addddca...........",
    "...........addddccba..........",
    "..........addddcccaa..........",
    ".........acddcccca24a.........",
    "........aacccccca142a.........",
    ".......aa1accccba42adaa.......",
    "......ada3abccba11addda.......",
    "......adca2abbba21adddda......",
    ".....adccca2aba44accddda......",
    ".....acccccaaa231acccccaa.....",
    "....accbcdddda131acccbbba.....",
    "....abbbbddddca13accbbbbba....",
    "...a1abbbbddccba1aacbbbba3a...",
    "...a2abbbbbccbbaa3abbbaa21a...",
    "..a332abbbbcbbbba14aba431ada..",
    "..a22a4abbbbbbbaaa21a2aaadda..",
    "..aaaatabbbbbbaacda111acddcba.",
    "...acca2aabbbbadddca1accdccba.",
    "...abcca31abbadddcca4acccccba.",
    "...aabca3t1aaadcccca42acccbba.",
    ".aa.abbba4taacccccbba3aabbba..",
    "adca.abbbt21acccccbba11aabaa..",
    "adba.aabb11abbbcbbbba111aaaaa.",
    ".aa.aa.aa1abbbbbbbba11aaa.adba",
    "...adda..aaabbbbbbaaaaa...acba",
    "...acba....aaaaaaaa........aa.",
    "....aa........................",
    "..............................",
};
static const char* const ORE_ART_GOLD[ORE_H] = {
    "..............aaa.............",
    "............aacccaa...........",
    "............addddca...........",
    "...........addddccba..........",
    "..........addddcccaa..........",
    ".........acddcccca24a.........",
    "........aacccccca142a.........",
    ".......aa1accccbaw2adaa.......",
    "......ada3abccba11addda.......",
    "......adca2abbba21adddda......",
    ".....adccca2aba43accddda......",
    ".....acccccaaa231acccccaa.....",
    "....accbcdddda1w1acccbbba.....",
    "....abbbbddddca13accbbbbba....",
    "...a1abbbbddccba1aacbbbba3a...",
    "...a2abbbbbccbbaa3abbbaa21a...",
    "..a332abbbbcbbbba13aba431ada..",
    "..a22a3abbbbbbbaaa21a2aaadda..",
    "..aaaawabbbbbbaacda111acddcba.",
    "...acca2aabbbbadddca1accdccba.",
    "...abcca31abbadddcca3acccccba.",
    "...aabcaw31aaadcccca32acccbba.",
    ".aa.abbba31aacccccbba3aabbba..",
    "adca.abbb221acccccbba11aabaa..",
    "adba.aabb11abbbcbbbba111aaaaa.",
    ".aa.aa.aa1abbbbbbbba11aaa.adba",
    "...adda..aaabbbbbbaaaaa...acba",
    "...acba....aaaaaaaa........aa.",
    "....aa........................",
    "..............................",
};
static const char* const ORE_ART_COAL[ORE_H] = {
    "..............................",
    "..............................",
    "..............................",
    "..............................",
    "..............................",
    "..............................",
    ".............111111...........",
    "...........11www4211..........",
    "...........1wwww42211.........",
    "..........1wwwww44421.........",
    "........111ww444442211........",
    ".......11www4444442221........",
    "......11444w44444234411.......",
    ".....114444444422233441.......",
    ".....14443334442233333111.....",
    "....1111333343322333324421....",
    "...1www11123332223332444221...",
    "...1ww443123332223324444321...",
    "...133443312322223223333321...",
    "...133343311122222233333321...",
    "...12333333w111122233333321...",
    "...1233311wwww411223332221....",
    "....11231344ww441122222221....",
    "..11..1213444444421222211.11..",
    ".1w41.111333444222112211.1431.",
    ".1331...12233442221111...1321.",
    "..11.111.123332221....11..11..",
    ".....141..1222221....1441.....",
    ".....111...11111.....1421.....",
    "......................11......",
};
static const char* const ORE_ART_AMBER[ORE_H] = {
    "..............................",
    "..............................",
    "..............................",
    "..............................",
    "..............................",
    "..............................",
    "...............11111..........",
    "..............198771..........",
    "............11887661..........",
    "...........1441776661.........",
    "..........11441576651.........",
    "..........144331666131........",
    ".........1133333166111........",
    "........111233222151971.......",
    ".......14812222221188611......",
    "......1143112222118876771.....",
    ".....14433751221188876661.....",
    ".....133332161217777666111....",
    "....1113322171117777655141....",
    "....18712222233115665514441...",
    "....17771222211341155114331...",
    "....17661122114443155133321...",
    "....11668111144333111123221...",
    "..11.1177611233332216711211...",
    ".1441.15766133322221776111....",
    ".1321..116133122221117611.441.",
    "..111.1.1112221221111111.1421.",
    ".....141...1111111.....11.11..",
    ".....112..............132.....",
    ".......................1......",
};
static const char* const* const ORE_ART[OREM_N] = {ORE_ART_COPPER, ORE_ART_IRON, ORE_ART_SILVER, ORE_ART_GOLD, ORE_ART_COAL, ORE_ART_AMBER};
// the metal colours: dark edge, body, light, pale
static const u32 ORE_METAL[OREM_N][4] = {
    {HEX(0x9a4e34), HEX(0xd9805a), HEX(0xf0a27c), HEX(0xffcfb5)},  // copper: pinkish
    {HEX(0x7a3a26), HEX(0x6f7d92), HEX(0xa3b2c6), HEX(0xdfe9f2)},  // iron: steel blue-grey with a rust-brown edge
    {HEX(0x7f8ca2), HEX(0xc6d1e0), HEX(0xeaf1fa), HEX(0xffffff)},  // silver: bright white-blue
    {HEX(0xa86a14), HEX(0xe3a91c), HEX(0xffd23f), HEX(0xfff4a8)},  // gold: warm yellow
    {HEX(0x18151d), HEX(0x1f1e26), HEX(0x3a3842), HEX(0x5c5b64)},  // coal: black lumps
    {HEX(0x29181f), HEX(0x503833), HEX(0x725443), HEX(0xa78467)},  // bog amber: the brown peat stone (the amber itself is 5-9, below)
};

// The ore rock for one metal in one biome's stone (theme = a Biome id: the index into ROCK_PALS).
inline Img mkOreRock(int metal, int theme) {
    const u32* stone = ROCK_PALS[theme];
    Img im(ORE_W, ORE_H);
    for (int y = 0; y < ORE_H; y++)
        for (int x = 0; x < ORE_W; x++) {
            const char c = ORE_ART[metal][y][x];
            if (c == '.') continue;
            u32 col;
            if (c >= 'a' && c <= 'd') col = stone[c - 'a'];
            else if (c >= '1' && c <= '4') col = ORE_METAL[metal][c - '1'];
            else if (c >= '5' && c <= '9') {  // the bog amber: dark, body, bright, pale facet, glint
                static const u32 AMBER[5] = {HEX(0x9c5d16), HEX(0xc9791c), HEX(0xf6b92e), HEX(0xffe27a), HEX(0xfffbe0)};
                col = AMBER[c - '5'];
            } else if (c == 'p') col = HEX(0x4a9a82);
            else if (c == 'q') col = HEX(0x7fcdb0);
            else if (c == 'x') col = HEX(0xc9653a);
            else if (c == 'y') col = HEX(0xb5522f);
            else if (c == 'z') col = HEX(0xd9774a);
            else if (c == 't') col = HEX(0xdfeaf6);
            else col = metal == OREM_COAL ? HEX(0x9b9aa1) : HEX(0xffffff);  // w: a glint (coal: the shine on a lump)
            im.set(x, y, col);
        }
    return im;
}
