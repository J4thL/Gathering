// snow_props.hpp - the snowfield's small props, redrawn: four snowmen, the lamp, three stumps, three logs and the ground clutter
// (four frost cracks, four sets of animal tracks, four half-buried rocks). Painted in code like the rest of the art.
// Needs Img, HEX and OUT (the outline colour) from the includes before it.
#pragma once

// ------------------------------------------------------------------------------------------------------------------ snowmen
// Four looks on one 16 x 25 canvas, the foot at the bottom row: 0 the classic one (top hat, red scarf), 1 a knitted beanie and a green scarf,
// 2 a small one in a bucket hat, 3 an old one that has started to melt: leaning, a crooked hat, a puddle of slush round its foot.
inline Img mkSnowmanV(int look) {
    Img im(16, 25);
    const u32 L = HEX(0xffffff), M = HEX(0xe3ecf6), D = HEX(0xa9bdd4), COAL = HEX(0x2a2a30), STICK = HEX(0x5a3d24), NOSE = HEX(0xe8802a), NOSE2 = HEX(0xc9631a);
    if (look == 0) {
        im.line(4, 13, 1, 9, STICK); im.line(12, 13, 15, 9, STICK);
        im.set(1, 8, STICK); im.set(15, 8, STICK); im.set(2, 11, STICK); im.set(14, 11, STICK);
        im.ellipseShaded(8, 19, 5.8f, 5.0f, L, M, D);
        im.ellipseShaded(8, 12.5f, 4.4f, 3.9f, L, M, D);
        im.ellipseShaded(8, 7.5f, 3.5f, 3.3f, L, M, D);
        im.set(7, 7, COAL); im.set(10, 7, COAL);
        im.set(8, 8, NOSE); im.set(9, 8, NOSE); im.set(10, 8, NOSE2);
        im.set(6, 9, COAL); im.set(7, 10, COAL); im.set(9, 10, COAL); im.set(10, 9, COAL);
        im.rect(5, 10, 7, 1, HEX(0xc0392b)); im.rect(10, 11, 2, 3, HEX(0xc0392b)); im.set(10, 13, HEX(0x8f271c));
        im.rect(5, 4, 7, 1, HEX(0x24242c)); im.rect(6, 1, 5, 3, HEX(0x24242c)); im.rect(6, 3, 5, 1, HEX(0x8f271c));
        im.set(6, 1, HEX(0x4a4a58)); im.set(6, 2, HEX(0x4a4a58));
        im.set(8, 16, COAL); im.set(8, 19, COAL); im.set(8, 22, COAL);
    } else if (look == 1) {  // beanie and green scarf, stone eyes, a row of pebbles for a smile, a branch with twigs for each arm
        im.line(4, 14, 0, 10, STICK); im.line(12, 14, 15, 11, STICK);
        im.set(0, 9, STICK); im.set(1, 11, STICK); im.set(15, 10, STICK); im.set(14, 12, STICK);
        im.ellipseShaded(8, 19, 5.8f, 5.0f, L, M, D);
        im.ellipseShaded(8, 12.7f, 4.3f, 3.8f, L, M, D);
        im.ellipseShaded(8, 7.8f, 3.4f, 3.2f, L, M, D);
        im.set(7, 7, HEX(0x555b66)); im.set(10, 7, HEX(0x555b66));
        im.set(8, 8, NOSE); im.set(9, 8, NOSE); im.set(10, 8, NOSE2);
        im.set(6, 10, HEX(0x555b66)); im.set(8, 10, HEX(0x555b66)); im.set(9, 10, HEX(0x555b66)); im.set(11, 10, HEX(0x555b66));
        im.rect(5, 10, 7, 2, HEX(0x2f8f5a)); im.rect(5, 11, 7, 1, HEX(0x226a43)); im.rect(5, 12, 2, 3, HEX(0x2f8f5a)); im.set(5, 14, HEX(0x226a43));
        im.rect(5, 3, 7, 3, HEX(0x3a63b8)); im.rect(5, 5, 7, 1, HEX(0x2b4a8c));            // the beanie
        for (int x = 5; x < 12; x += 2) im.set(x, 4, HEX(0x7a9be0));
        im.ellipse(8, 2, 1.6f, 1.4f, HEX(0xf4f8fc));                                        // a pompom
        im.set(8, 17, HEX(0x555b66)); im.set(8, 20, HEX(0x555b66)); im.set(8, 23, HEX(0x555b66));
    } else if (look == 2) {  // a small one: two balls, a bucket hat, a long carrot
        im.line(5, 18, 2, 15, STICK); im.line(11, 18, 14, 16, STICK);
        im.set(1, 14, STICK); im.set(15, 15, STICK); im.set(3, 17, STICK);
        im.ellipseShaded(8, 20.2f, 5.0f, 4.3f, L, M, D);
        im.ellipseShaded(8, 13.4f, 3.7f, 3.4f, L, M, D);
        im.set(7, 13, COAL); im.set(10, 13, COAL);
        im.set(8, 14, NOSE); im.set(9, 14, NOSE); im.set(10, 14, NOSE); im.set(11, 14, NOSE2); im.set(12, 14, NOSE2);
        im.set(6, 15, COAL); im.set(7, 16, COAL); im.set(9, 16, COAL); im.set(10, 15, COAL);
        im.rect(5, 9, 7, 3, HEX(0x8d929e)); im.rect(5, 9, 7, 1, HEX(0xb4b9c4)); im.rect(4, 11, 9, 1, HEX(0x6a6f7c));  // a tin bucket for a hat
        im.set(6, 10, HEX(0xffffff));
        im.rect(6, 17, 5, 1, HEX(0xd8a82a)); im.set(10, 18, HEX(0xd8a82a));          // a yellow scarf
        im.set(8, 20, COAL); im.set(8, 22, COAL);
        im.set(13, 21, HEX(0xa9bdd4)); im.set(3, 22, HEX(0xa9bdd4));
    } else {  // melting: wide and slumped, leaning left, a crooked hat, one arm fallen off, slush on the ground
        im.ellipse(8, 22.6f, 7.8f, 2.3f, HEX(0xb9cce3));                             // the slush it is standing in
        im.ellipse(8, 22.8f, 7.0f, 1.7f, HEX(0x9db6d2));
        im.line(3, 15, 0, 17, STICK); im.set(0, 18, STICK); im.set(1, 16, STICK);   // an arm drooping
        im.ellipseShaded(8.5f, 19.5f, 6.4f, 4.2f, L, M, D);
        im.ellipseShaded(7.5f, 13.6f, 4.3f, 3.5f, L, M, D);
        im.ellipseShaded(6.3f, 8.6f, 3.4f, 3.0f, L, M, D);
        im.set(5, 8, COAL); im.set(8, 7, COAL);                                      // uneven eyes
        im.set(6, 9, NOSE); im.set(7, 9, NOSE); im.set(8, 10, NOSE2);                // a drooping nose
        im.set(4, 11, COAL); im.set(5, 12, COAL); im.set(6, 12, COAL); im.set(7, 11, COAL);
        im.rect(3, 11, 8, 1, HEX(0xc0392b)); im.rect(8, 12, 2, 2, HEX(0xc0392b));
        im.line(2, 5, 10, 3, HEX(0x24242c)); im.rect(4, 2, 5, 2, HEX(0x24242c)); im.rect(3, 4, 7, 1, HEX(0x24242c)); im.set(5, 3, HEX(0x8f271c));  // a tilted hat
        im.set(13, 12, STICK); im.set(14, 11, STICK); im.set(12, 13, STICK);         // the other arm's twig, still in
        im.set(13, 18, HEX(0xa9bdd4)); im.set(3, 21, HEX(0x9db6d2));
        im.set(6, 14, COAL); im.set(7, 18, COAL);
        im.set(2, 23, HEX(0xdbe6f2)); im.set(12, 23, HEX(0xdbe6f2));                 // drips
    }
    im.outline(OUT);
    return im;
}

// ------------------------------------------------------------------------------------------------------------------ the lamp
// A stout iron post on a stone foot with a glass lantern: a thick snow cap with icicles under it, frost on the post, a drift round the foot.
// 10 x 36, the foot on the bottom rows, like the old one (the lantern's light is placed from the same anchor).
inline Img mkSnowLampV() {
    Img im(10, 36);
    const u32 W = HEX(0xf4f8fc), W2 = HEX(0xdbe6f2), IRON = HEX(0x2e2e38), IRON2 = HEX(0x4a4a58), IRON3 = HEX(0x1c1c24);
    im.rect(4, 11, 2, 19, IRON); im.rect(4, 11, 1, 19, IRON2);             // the post, lit on its left
    im.rect(3, 12, 4, 1, IRON2); im.rect(3, 21, 4, 1, IRON2);              // two collars
    im.rect(2, 29, 6, 6, HEX(0x6a6f7c)); im.rect(2, 29, 6, 1, HEX(0x9aa0ae)); im.rect(2, 34, 6, 1, HEX(0x4a4f5c));  // the stone foot
    im.rect(5, 31, 1, 2, HEX(0x555b66)); im.rect(3, 32, 1, 1, HEX(0x555b66));
    im.rect(1, 3, 8, 9, IRON);                                             // the lantern's frame
    im.rect(2, 4, 6, 7, HEX(0xffe9a8)); im.rect(3, 5, 4, 5, HEX(0xfff6d0)); im.rect(4, 6, 2, 3, HEX(0xffffff));
    im.rect(2, 7, 6, 1, IRON3); im.rect(4, 4, 2, 7, IRON3);                // glazing bars
    im.rect(4, 5, 2, 1, HEX(0xffe9a8)); im.rect(4, 9, 2, 1, HEX(0xffe9a8));
    im.rect(2, 11, 6, 1, IRON3);
    im.rect(0, 1, 10, 3, W); im.rect(1, 0, 8, 1, W); im.rect(0, 3, 10, 1, W2);  // a deep cap of snow with an overhang
    im.set(3, 1, HEX(0xffffff)); im.set(6, 2, HEX(0xffffff));
    im.set(1, 4, HEX(0xcfe6ff)); im.set(4, 4, HEX(0xcfe6ff)); im.set(8, 4, HEX(0xcfe6ff)); im.set(8, 5, HEX(0xcfe6ff));  // icicles
    im.rect(3, 13, 1, 2, W2); im.set(5, 17, W2); im.set(4, 23, W2);        // frost clinging to the post
    im.rect(0, 33, 10, 2, W); im.rect(1, 32, 3, 1, W); im.rect(7, 32, 2, 1, W2); im.rect(2, 35, 6, 1, W2);  // a drift round the foot
    im.outline(OUT);
    return im;
}

// ------------------------------------------------------------------------------------------------------------------ stumps
// 14 x 11, the foot at row 9: 0 a tall stump under a pad of snow, 1 a wide low stump with its rings showing under half a pad of snow,
// 2 a broken stump: splintered top, a dark hollow, snow heaped in the break.
inline Img mkSnowStumpV(int look) {
    Img im(14, 11);
    const u32 W = HEX(0xf4f8fc), W2 = HEX(0xdbe6f2), B1 = HEX(0x8a5a2c), B2 = HEX(0x6a4220), B3 = HEX(0x4a2c14), CUT = HEX(0xe0b878), CUT2 = HEX(0xc89a58);
    if (look == 0) {
        im.rect(2, 4, 10, 6, B2); im.rect(2, 4, 3, 6, B1); im.rect(10, 4, 2, 6, B3);
        im.set(3, 9, B3); im.set(7, 6, B3); im.set(8, 7, B3); im.set(6, 8, B3);               // bark
        im.set(1, 9, B2); im.set(12, 9, B2); im.set(0, 10, B3); im.set(13, 10, B3);           // root flares
        im.ellipse(7, 4, 5.4f, 2.8f, W);
        im.ellipse(7, 4.5f, 3.6f, 1.6f, W2);
        im.set(5, 3, HEX(0xffffff)); im.set(9, 4, HEX(0xffffff));
        im.rect(2, 8, 3, 1, W2); im.rect(9, 9, 3, 1, W);
    } else if (look == 1) {
        im.rect(1, 5, 12, 5, B2); im.rect(1, 5, 4, 5, B1); im.rect(11, 5, 2, 5, B3);
        im.set(4, 8, B3); im.set(6, 9, B3); im.set(9, 7, B3); im.set(3, 6, B3);
        im.set(0, 9, B2); im.set(13, 9, B2); im.set(0, 10, B3); im.set(13, 10, B3); im.set(1, 10, B3); im.set(12, 10, B3);
        im.ellipse(7, 5, 6.2f, 3.0f, CUT);                                                    // the cut face with its rings...
        im.ellipse(7, 5, 4.4f, 2.0f, CUT2);
        im.ellipse(7, 5, 2.6f, 1.1f, CUT);
        im.set(7, 5, B1);
        im.ellipse(5, 4, 4.0f, 2.0f, W);                                                      // ...and snow over its back half
        im.ellipse(5, 4.4f, 2.6f, 1.2f, W2);
        im.rect(8, 9, 4, 1, W); im.rect(1, 9, 3, 1, W2);
    } else {
        im.rect(2, 4, 10, 6, B2); im.rect(2, 4, 3, 6, B1); im.rect(10, 4, 2, 6, B3);
        im.set(5, 7, B3); im.set(6, 8, B3); im.set(8, 6, B3); im.set(4, 9, B3);
        const int top[10] = {4, 2, 3, 1, 3, 4, 2, 3, 1, 4};                                    // a ragged broken top
        for (int i = 0; i < 10; i++) im.rect(2 + i, top[i], 1, 4 - top[i] + 1, i % 3 == 0 ? B1 : B2);
        im.rect(4, 4, 6, 2, B3);                                                              // the dark hollow
        im.rect(5, 3, 4, 2, W); im.rect(4, 4, 6, 1, W2); im.set(6, 2, W); im.set(7, 3, HEX(0xffffff));  // snow heaped in the break
        im.set(3, 3, W); im.set(11, 3, W2);
        im.set(2, 5, HEX(0xcfe6ff)); im.set(11, 5, HEX(0xcfe6ff)); im.set(11, 6, HEX(0xcfe6ff));  // icicles off the splinters
        im.rect(2, 9, 3, 1, W2); im.rect(9, 9, 3, 1, W);
        im.set(1, 9, B2); im.set(12, 9, B2); im.set(0, 10, B3); im.set(13, 10, B3);
    }
    im.outline(OUT);
    return im;
}

// ------------------------------------------------------------------------------------------------------------------ logs
// 20 x 10, the foot at row 8: 0 a log with a sawn end showing its rings, 1 a log with a knot and a snow-laden branch stub, 2 two logs, one on the other.
inline Img mkSnowLogV(int look) {
    Img im(20, 10);
    const u32 W = HEX(0xf4f8fc), W2 = HEX(0xdbe6f2), B1 = HEX(0x8d5e33), B2 = HEX(0x6b4423), B3 = HEX(0x52341a), CUT = HEX(0xc99a5a), CUT2 = HEX(0x8a5c34);
    if (look == 0) {
        im.rect(2, 3, 17, 5, B2); im.rect(2, 3, 17, 1, B1); im.rect(2, 7, 17, 1, B3);
        for (int x = 5; x < 18; x += 4) { im.set(x, 5, B3); im.set(x + 1, 5, B3); im.set(x + 2, 6, B3); }  // bark furrows
        im.set(10, 4, B1); im.set(15, 4, B1);
        im.ellipse(2.5f, 5.5f, 1.9f, 2.7f, CUT);                                              // the sawn end: two rings and a heart
        im.ellipse(2.5f, 5.5f, 1.1f, 1.7f, CUT2);
        im.set(2, 5, CUT);
        im.rect(5, 2, 13, 1, W); im.rect(4, 1, 11, 1, W); im.rect(6, 3, 8, 1, W2);
        im.set(16, 3, W2); im.set(17, 3, W);
        im.rect(3, 8, 16, 1, W2); im.rect(8, 9, 7, 1, W);
    } else if (look == 1) {
        im.rect(1, 4, 18, 4, B2); im.rect(1, 4, 18, 1, B1); im.rect(1, 7, 18, 1, B3);
        im.ellipse(15, 5.5f, 1.4f, 1.1f, B3); im.set(15, 5, B1);                               // a knot
        im.rect(11, 1, 2, 3, B2); im.set(11, 1, B1); im.set(12, 0, B1);                        // a branch stub standing up...
        im.rect(10, 0, 4, 1, W); im.rect(11, 1, 2, 1, W2);                                     // ...with its own cap of snow
        for (int x = 3; x < 10; x += 3) { im.set(x, 6, B3); im.set(x + 1, 6, B3); }
        im.ellipse(1.5f, 6, 1.3f, 2.0f, CUT); im.set(1, 6, CUT2);
        im.rect(2, 3, 8, 1, W); im.rect(14, 3, 4, 1, W); im.rect(4, 2, 4, 1, W2); im.rect(15, 2, 2, 1, W);
        im.rect(2, 8, 16, 1, W2); im.rect(6, 9, 8, 1, W);
    } else {
        im.rect(1, 5, 17, 3, B2); im.rect(1, 5, 17, 1, B1); im.rect(1, 7, 17, 1, B3);          // the lower log
        im.rect(4, 2, 12, 3, B1); im.rect(4, 2, 12, 1, HEX(0xa8723f)); im.rect(4, 4, 12, 1, B2);  // the upper one, shorter, on top of it
        im.ellipse(1.5f, 6.3f, 1.3f, 1.8f, CUT); im.set(1, 6, CUT2);
        im.ellipse(4.5f, 3.4f, 1.2f, 1.6f, CUT); im.set(4, 3, CUT2);
        for (int x = 7; x < 16; x += 3) im.set(x, 3, B3);
        for (int x = 4; x < 16; x += 4) im.set(x, 6, B3);
        im.rect(5, 1, 10, 1, W); im.rect(6, 0, 7, 1, W); im.rect(16, 4, 3, 1, W); im.rect(2, 4, 2, 1, W2);
        im.rect(2, 8, 16, 1, W2); im.rect(7, 9, 7, 1, W);
    }
    im.outline(OUT);
    return im;
}

// ------------------------------------------------------------------------------------------------------------------ ground clutter
// Frost cracks: a dark blue line in the snow crust with a bright lip, 18 x 10: 0 a star, 1 a long zigzag, 2 a fork, 3 two hairline cracks with chips of ice.
inline Img mkFrostCrackV(int v) {
    Img im(18, 10);
    const u32 CR = HEX(0x6d88ad), CR2 = HEX(0x8aa2c2), HI = HEX(0xffffff);
    auto seg = [&](int x0, int y0, int x1, int y1) {
        im.line(x0, y0, x1, y1, CR);
        im.line(x0 + 1, y0 + 1, x1 + 1, y1 + 1, HI);   // the lit lip on the far side of the crack
    };
    if (v == 0) {
        seg(9, 5, 3, 3); seg(9, 5, 15, 4); seg(9, 5, 7, 9); seg(9, 5, 11, 0);
        im.set(9, 5, HEX(0x4a6385)); im.set(4, 2, CR2); im.set(14, 3, CR2); im.set(6, 8, CR2);
    } else if (v == 1) {
        seg(1, 4, 4, 6); seg(4, 6, 8, 3); seg(8, 3, 12, 6); seg(12, 6, 16, 3);
        im.line(8, 3, 9, 0, CR); im.line(12, 6, 13, 9, CR);
        im.set(1, 4, HEX(0x4a6385)); im.set(16, 3, CR2);
    } else if (v == 2) {
        seg(2, 8, 8, 5); seg(8, 5, 12, 1); seg(8, 5, 15, 7);
        im.line(5, 7, 4, 4, CR); im.line(12, 3, 16, 2, CR2);
        im.set(2, 8, HEX(0x4a6385));
    } else {
        seg(1, 3, 7, 4); seg(7, 4, 10, 2); seg(5, 8, 11, 7); seg(11, 7, 16, 8);
        im.set(13, 3, HI); im.set(14, 3, HI); im.set(14, 4, CR2); im.set(3, 6, HI); im.set(2, 6, CR2); im.set(9, 5, HI);  // chips of ice
    }
    return im;
}

// Animal tracks, 20 x 10: 0 a fox (small prints in a line), 1 a deer (pairs of cloven hoof marks), 2 a bird (three-toed arrows), 3 a hare (two long hind
// prints side by side, two small ones before them).
inline Img mkTracksV(int v) {
    Img im(20, 10);
    const u32 SH = HEX(0x7f9abd), SH2 = HEX(0x98b0cd), HI = HEX(0xffffff);
    auto paw = [&](int x, int y) {
        im.set(x + 1, y + 2, SH); im.set(x + 2, y + 2, SH); im.set(x + 1, y + 3, SH); im.set(x + 2, y + 3, SH);
        im.set(x, y + 1, SH); im.set(x + 1, y, SH); im.set(x + 2, y, SH); im.set(x + 3, y + 1, SH);
        im.set(x + 1, y + 1, HI); im.set(x + 2, y + 4, SH2);
    };
    auto hoof = [&](int x, int y) {  // two crescents side by side, pointing up-line
        im.set(x, y, SH); im.set(x, y + 1, SH); im.set(x + 1, y + 2, SH); im.set(x + 1, y + 1, HI);
        im.set(x + 3, y, SH); im.set(x + 3, y + 1, SH); im.set(x + 2, y + 2, SH); im.set(x + 2, y + 1, HI);
    };
    auto bird = [&](int x, int y) {
        im.set(x + 2, y, SH); im.set(x + 2, y + 1, SH); im.set(x + 2, y + 2, SH);                         // the middle toe
        im.set(x, y + 3, SH); im.set(x + 1, y + 2, SH); im.set(x + 4, y + 3, SH); im.set(x + 3, y + 2, SH);  // the side toes
        im.set(x + 2, y + 3, SH2);
    };
    if (v == 0) { paw(0, 5); paw(5, 1); paw(10, 5); paw(15, 1); }
    else if (v == 1) { hoof(1, 5); hoof(5, 4); hoof(10, 1); hoof(14, 0); im.set(7, 8, SH2); im.set(16, 4, SH2); }
    else if (v == 2) { bird(1, 5); bird(8, 3); bird(14, 5); im.line(6, 7, 8, 6, SH2); im.line(12, 6, 14, 7, SH2); }
    else {
        im.rect(1, 5, 2, 4, SH); im.rect(1, 5, 2, 1, SH2); im.rect(4, 4, 2, 4, SH); im.rect(4, 4, 2, 1, SH2);   // hind prints
        im.set(2, 6, HI); im.set(5, 5, HI);
        im.set(10, 4, SH); im.set(11, 4, SH); im.set(10, 5, SH2); im.set(13, 2, SH); im.set(14, 2, SH); im.set(13, 3, SH2);  // front prints
        im.rect(17, 0, 2, 1, SH2); im.set(18, 1, SH2);
    }
    return im;
}

// Half-buried rocks, with a soft drift of snow against them: 0 a round one, 1 a pointed one, 2 two together, 3 a flat slab. About 14 x 10.
inline Img mkBuriedRockV(int v) {
    Img im(14, 10);
    const u32 W = HEX(0xf4f8fc), W2 = HEX(0xdbe6f2), W3 = HEX(0xb9cce3);
    const u32 G1 = HEX(0xa6acba), G2 = HEX(0x838a9a), G3 = HEX(0x606677);
    if (v == 0) {
        im.ellipse(7, 8, 6.4f, 1.6f, W3);                                                   // the drift
        im.ellipseShaded(7, 5.6f, 4.9f, 3.4f, G1, G2, G3);
        im.ellipse(6.4f, 3.4f, 3.4f, 1.6f, W); im.set(10, 4, W2); im.set(3, 4, W2);
        im.set(4, 6, G3); im.set(9, 7, G3);
        im.rect(3, 8, 8, 1, W2);
    } else if (v == 1) {
        im.ellipse(7, 8, 6.0f, 1.5f, W3);
        for (int y = 1; y <= 8; y++) {                                                      // a pointed outcrop
            const float hw = 0.8f + (y - 1) * 0.55f;
            for (int x = (int)(7 - hw); x <= (int)(7 + hw); x++) im.set(x, y, x < 7 ? G1 : (x == (int)(7 + hw) ? G3 : G2));
        }
        im.rect(6, 1, 3, 2, W); im.set(5, 3, W); im.set(9, 3, W2); im.set(7, 4, W2);
        im.set(8, 6, G3); im.set(6, 7, G3);
        im.rect(3, 8, 8, 1, W2);
    } else if (v == 2) {
        im.ellipse(7, 8, 6.6f, 1.6f, W3);
        im.ellipseShaded(4.6f, 6.2f, 3.4f, 2.6f, G1, G2, G3);
        im.ellipseShaded(9.6f, 5.4f, 3.6f, 3.2f, G1, G2, G3);
        im.ellipse(4.4f, 4.6f, 2.2f, 1.0f, W); im.ellipse(9.4f, 3.4f, 2.6f, 1.3f, W); im.set(12, 5, W2);
        im.rect(2, 8, 10, 1, W2);
    } else {
        im.ellipse(7, 8, 6.8f, 1.5f, W3);
        im.ellipseShaded(7, 6.6f, 6.2f, 2.4f, G1, G2, G3);
        im.ellipse(7, 5, 5.0f, 1.2f, W); im.rect(2, 5, 3, 1, W2); im.set(11, 6, W2);
        im.set(5, 7, G3); im.set(9, 7, G3);
        im.rect(1, 8, 12, 1, W2);
    }
    return im;
}
