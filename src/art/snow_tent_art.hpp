// snow_tent_art.hpp - the snowfield's tent, hand-drawn (cell-shaded, its own dark outline): 32 x 32, the foot at the bottom rows.
// Rows of palette letters, '.' is transparent; the palette is below. Needs Img and HEX from the includes before it.
#pragma once

inline Img mkSnowTentArt() {
    static const char* const ROWS[32] = {
        "...............aa...............",
        "..............aaaa..............",
        "..............abca..............",
        "..............acba..............",
        ".............adceda.............",
        "............adfeffda............",
        "............addffdda............",
        "............aghddhga............",
        "...........adihhhhida...........",
        "..........adidhhhhdida..........",
        ".........adidhhhhhhdida.........",
        "........adigdhhhhhhdgid.........",
        "........adgdhhhhhhhhdgda........",
        ".......adigihhhddhhhigida.......",
        "......adiighhhiddihhhgiida......",
        "......agigihhhidgihhhigiga......",
        ".....adiggihhhiddihhhiggida.....",
        "....adiigghhhiiddiihhhggiida....",
        "...adiiggihhhigddgihhhiggiida...",
        ".aadgiggghhhiidaadiihhhgggigdaa.",
        "abbfggggihhhiggaaggihhhiggggfbba",
        "accfdggdhhhiigiaaigiihhhdggdfcca",
        ".afdgggihhiigdgaagdgiihhigggefa.",
        "..adggghhhiididaadidiihhhgggda..",
        "..adgdhhhiigdiaaaaidgiihhhdgda..",
        "..addihiiigdidaaaadidgiiihidda..",
        "..afdhiiigggiaaaaaaigggiiihdfa..",
        ".abbfiigggdidaffffadidgggiifbba.",
        ".abcfggggdiiafccccfaiidggggfcba.",
        "..affddgggigfccccccfaigggddffa..",
        "...afaaaagiaaaaaaaaaaigaaaafa...",
        "....aa...aaa........aaa...aa....",
    };
    auto col = [](char c) -> u32 {
        switch (c) {
        case 'a': return HEX(0x201527);
        case 'b': return HEX(0xc48254);
        case 'c': return HEX(0x98563e);
        case 'd': return HEX(0x976863);
        case 'e': return HEX(0x7a4739);
        case 'f': return HEX(0x58372e);
        case 'g': return HEX(0xcda084);
        case 'h': return HEX(0xf9e7bb);
        case 'i': return HEX(0xebc79d);
            default: return 0;
        }
    };
    Img im(32, 32);
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            if (ROWS[y][x] != '.') im.set(x, y, col(ROWS[y][x]));
    return im;
}

// The same tent under a heavy cover of snow, hand-drawn like the first (32 x 32, anchored the same): the snowfield's second look for a standing tent.
inline Img mkSnowTentSnowyArt() {
    static const char* const ROWS[32] = {
        "...............aa...............",
        "..............abba..............",
        "..............acda..............",
        "..............edce..............",
        ".............afdgfa.............",
        "............ahhgiiha............",
        "............ahhhihha............",
        "...........abhhhhhhha...........",
        "..........abhhbbhhhhha..........",
        ".........abbhbbaabhhhba.........",
        "........abhbbbajjabhhhba........",
        ".......abhhhbajjjjahhbha........",
        ".......ahhhbbajjjjabbbhba.......",
        "......abhhhbajjffjjabhhhba......",
        ".....abhhhbajjkffkjjabhhhba.....",
        ".....ahhhhbajjkflkjjabhhhba.....",
        "....abbaabajjjkffkjjjabhhba.....",
        "....aaakabajjkkffkkjjabbbaaa....",
        ".aaabakllajjjklffhkjjjabahbbaaa.",
        "ahhbakllljjjkkfeeahkjjjaaahbbaha",
        "bccillllkjjjklleeabkjjjallaaibba",
        "eddifllfjjjkklkeekakkjjjfllfidda",
        ".eiflllkjjkklfleelflkkjjklllgie.",
        "..efllljjjkhakfeefkfkkjjjlllfe..",
        "..eflfjjjkhhakeeeekflkkjjjflfe..",
        ".aaafkjkhhhakfeeeefkflkkkjkfaa..",
        "abbhfjkkkhbakeeeeeeklhhkkkjfhha.",
        ".accikklllakfeiiiiefkahhhkkicba.",
        ".ecdillllakkeiddddiekkabhhlidaa.",
        "..eiifflalkliddddddieklabafiie..",
        "...eieeeelkeeeeeeeeeekleeeeie...",
        "....ee...eee........eee...ee....",
    };
    auto col = [](char c) -> u32 {
        switch (c) {
            case 'a': return HEX(0xaac5f0);
            case 'b': return HEX(0xc8dbf8);
            case 'c': return HEX(0xc48254);
            case 'd': return HEX(0x98563e);
            case 'e': return HEX(0x201527);
            case 'f': return HEX(0x976863);
            case 'g': return HEX(0x7a4739);
            case 'h': return HEX(0xe7effb);
            case 'i': return HEX(0x58372e);
            case 'j': return HEX(0xf9e7bb);
            case 'k': return HEX(0xebc79d);
            case 'l': return HEX(0xcda084);
            default: return 0;
        }
    };
    Img im(32, 32);
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            if (ROWS[y][x] != '.') im.set(x, y, col(ROWS[y][x]));
    return im;
}
