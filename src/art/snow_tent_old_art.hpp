// snow_tent_old_art.hpp - the snowfield's tent that has come down, hand-drawn (cell-shaded, its own dark outline): 32 x 32, anchored like the
// standing tent at its foot. Two looks: 0 under a covering of snow (the snowfield's), 1 bare (for the other biomes' abandoned camps).
// Rows of palette letters, '.' is transparent. Needs Img and HEX from the includes before it.
#pragma once

inline Img mkSnowTentOldArt(int look) {
    static const char* const SNOWY[32] = {
        "..................a.............",
        ".................abc............",
        "................adbe............",
        "...............adfa.............",
        "...............afga.............",
        "..............affa..............",
        ".............adfffa.............",
        ".............afddffa............",
        "............adfhdhfa............",
        "...........adfijdfffa...........",
        "..........adfkjjdfhafa..........",
        "......aaaadfhihidfhjfda.........",
        ".....aadddfhkihidfhfffda........",
        "...aaddfflhkdhhiddkhfffdaa...a..",
        "..addffllhkidahiddihffffddaaada.",
        "..affhfldhiddaaiddfkhfhhffdddfa.",
        "...aahfdhdddkaaidddkhfaahjfffga.",
        ".....adhdkdiaaaidlddkhjajhjhaa..",
        "....afhiiddkaaakdakdihfdhfha....",
        ".a.adhikmdiaaaakdkalikfddfha..a.",
        "adaaakhnddkaaaaaddalkikfdfha.ada",
        "aaafaaaadiaaaaaakdoolikjffafada.",
        ".addfdfaakaaaoooadoohkikjhaffafa",
        "...fddffapooqqqqaddkkkkaaaafdaf.",
        "...aa..aaaraaaaoakiihkkabaafada.",
        "..addaadqoajijaaahhaahhsbafaqa..",
        "...aadgaaaiidkaafaafdaaagafaga..",
        "..adfaakaaidaaafddaaddffaffaa...",
        ".......aa.aafdddaagbaaadd.......",
        "................adaagbba........",
        ".................aa.aaa.........",
        "................................",
    };
    static const char* const BARE[32] = {
        "..................a.............",
        ".................abc............",
        "................adbe............",
        "...............afga.............",
        "...............agda.............",
        "..............agda..............",
        ".............afffha.............",
        ".............aihhiha............",
        "............ahjhjhia............",
        "...........akhjijkiia...........",
        "..........akhkiijkhaha..........",
        "......aaaahkhjhjjkhiiia.........",
        ".....aahkjkhkjhjjjhiiiia........",
        "...aahikklhkjhhjjjkhiiihaa...a..",
        "..agfiillhkjjahjjjjhiiiiiiaaaga.",
        "..adfhilihjjkaajjjjkhihhihiifga.",
        "...aahiihjjjkaajjjjkhiaahihifda.",
        ".....aihjkjjaaajjljkkhiaihihaa..",
        "....aihjjjjkaaakjakkjhiihiha....",
        ".a.ahhjkmjjaaaakjkaljkiiiiha..a.",
        "abahkkhnjjkaaaaajjalkjkiiiha.ada",
        "adfhhmmmjjaaaaaakjffljkiiia.ada.",
        ".aaaahjjjkaaafffajffhkjkiha..a..",
        ".....aaakoffggggajjkkkkaaaa..a..",
        "...aa..aaapaaaafakjjhkkabaa.aga.",
        "..ajjaadgfaijiaaahhaahhqba.aga..",
        "...aagdaaajjjkaa.aa..aaada.ada..",
        ".....aakaajjaaa..aaa....a..aa...",
        ".......aa.aa....aagbaaa.........",
        "................akaagbba........",
        ".................aa.aaa.........",
        "................................",
    };
    auto snowy = [](char c) -> u32 {
        switch (c) {
            case 'a': return HEX(0x211526);
            case 'b': return HEX(0xc48254);
            case 'c': return HEX(0x221627);
            case 'd': return HEX(0xf6faff);
            case 'e': return HEX(0x211424);
            case 'f': return HEX(0xb2c4e0);
            case 'g': return HEX(0x7a4739);
            case 'h': return HEX(0x976863);
            case 'i': return HEX(0xf9e7bb);
            case 'j': return HEX(0xcda084);
            case 'k': return HEX(0xebc79d);
            case 'l': return HEX(0x221527);
            case 'm': return HEX(0x221525);
            case 'n': return HEX(0x201324);
            case 'o': return HEX(0x58372e);
            case 'p': return HEX(0x1f1525);
            case 'q': return HEX(0x98563e);
            case 'r': return HEX(0x1f1423);
            case 's': return HEX(0x221626);
            default: return 0;
        }
    };
    auto bare = [](char c) -> u32 {
        switch (c) {
            case 'a': return HEX(0x211526);
            case 'b': return HEX(0xc48254);
            case 'c': return HEX(0x221627);
            case 'd': return HEX(0x7a4739);
            case 'e': return HEX(0x211424);
            case 'f': return HEX(0x58372e);
            case 'g': return HEX(0x98563e);
            case 'h': return HEX(0x976863);
            case 'i': return HEX(0xcda084);
            case 'j': return HEX(0xf9e7bb);
            case 'k': return HEX(0xebc79d);
            case 'l': return HEX(0x221527);
            case 'm': return HEX(0x221525);
            case 'n': return HEX(0x201324);
            case 'o': return HEX(0x1f1525);
            case 'p': return HEX(0x1f1423);
            case 'q': return HEX(0x221626);
            default: return 0;
        }
    };
    const char* const* rows = look == 0 ? SNOWY : BARE;
    Img im(32, 32);
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            if (rows[y][x] != '.') im.set(x, y, look == 0 ? snowy(rows[y][x]) : bare(rows[y][x]));
    return im;
}
