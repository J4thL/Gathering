// snow_landmarks.hpp - the snowfield's landmark dressing: the Watch Cairn's lookout tower, the Ice Fisher's drying rack, firewood and
// fishing holes, and the camp variants (a broken tent, a cold fire ring). Procedural pixel art in the same palette as sprites_biome.hpp.
#pragma once
#include "core/gfx.hpp"

namespace snowlm {
const u32 W = HEX(0xf4f8fc), W2 = HEX(0xdbe6f2), BL = HEX(0xaec3da), OUTL = HEX(0x241a14);
}

// The lookout (32 x 64, anchored at the middle of its foot): a stone tower with a timber platform and a brazier burning on top.
inline Img mkLookout() {
    using namespace snowlm;
    Img im(32, 64);
    const u32 S1 = HEX(0x9aa2b2), S2 = HEX(0x7b8496), S3 = HEX(0x5c6578), MORT = HEX(0x4a5266);
    const u32 WOOD = HEX(0x8d5e33), WOOD2 = HEX(0x6b4423), WOOD3 = HEX(0x52341a);
    // the shaft: a tapering stone tower, lit on the left
    for (int y = 22; y <= 61; y++) {
        const float t = (y - 22) / 39.f;
        const int hw = 7 + (int)(t * 4.f);  // 7 at the top, 11 at the foot
        for (int x = 16 - hw; x <= 16 + hw; x++) im.set(x, y, x < 14 ? S1 : (x < 19 ? S2 : S3));
    }
    for (int y = 24; y <= 61; y += 4) {  // courses of masonry
        const float t = (y - 22) / 39.f;
        const int hw = 7 + (int)(t * 4.f);
        for (int x = 16 - hw; x <= 16 + hw; x++) im.set(x, y, MORT);
        for (int x = 16 - hw + ((y / 4) % 2 ? 2 : 5); x <= 16 + hw; x += 6)
            if (y + 1 <= 61) { im.set(x, y + 1, MORT); im.set(x, y + 2, MORT); }
    }
    im.rect(14, 54, 5, 8, HEX(0x24180f));  // the door
    im.rect(15, 53, 3, 1, HEX(0x24180f));
    im.set(13, 58, WOOD); im.set(19, 58, WOOD2);
    im.rect(12, 34, 2, 4, HEX(0x1a1420)); im.rect(19, 40, 2, 4, HEX(0x1a1420));  // two arrow slits
    // snow lying on the footing and in the courses
    im.rect(4, 59, 24, 3, W); im.rect(6, 58, 8, 1, W); im.rect(19, 58, 7, 1, W2); im.rect(5, 61, 22, 1, W2);
    for (int y = 28; y <= 52; y += 8) { im.set(10 - (y - 28) / 8, y, W); im.set(11 - (y - 28) / 8, y, W2); }
    // the platform: a timber deck jutting out round the top, with a rail
    im.rect(4, 17, 24, 3, WOOD); im.rect(4, 17, 24, 1, HEX(0xa8723d)); im.rect(4, 19, 24, 1, WOOD3);
    for (int x = 6; x <= 26; x += 4) im.rect(x, 20, 2, 3, WOOD2);  // brackets under the deck
    im.rect(4, 11, 1, 6, WOOD2); im.rect(27, 11, 1, 6, WOOD2); im.rect(15, 11, 2, 6, WOOD2);  // rail posts
    im.rect(4, 11, 24, 1, WOOD); im.rect(4, 12, 24, 1, WOOD3);
    im.rect(4, 10, 24, 1, W); im.rect(5, 9, 4, 1, W); im.rect(22, 9, 5, 1, W2);  // snow along the rail
    // the brazier: an iron bowl on a stand, the fire above it
    im.rect(12, 14, 8, 2, HEX(0x3a3d46)); im.rect(13, 13, 6, 1, HEX(0x555966));
    im.rect(10, 12, 12, 1, HEX(0x555966)); im.rect(11, 13, 10, 1, HEX(0x2a2c34));
    im.rect(13, 6, 6, 6, HEX(0xff7a22));
    im.rect(12, 8, 8, 4, HEX(0xff9a3c));
    im.rect(14, 3, 4, 5, HEX(0xffb347));
    im.rect(15, 1, 2, 4, HEX(0xffd98a));
    im.rect(14, 8, 4, 4, HEX(0xffe9a8));
    im.rect(15, 9, 2, 2, HEX(0xffffd0));
    im.outline(OUTL);
    return im;
}

// The drying rack (26 x 22, anchored at the middle of its foot): two posts, a bar, and a row of fish hanging by their tails.
inline Img mkFishRack() {
    using namespace snowlm;
    Img im(26, 22);
    const u32 WOOD = HEX(0x8d5e33), WOOD2 = HEX(0x6b4423), WOOD3 = HEX(0x52341a);
    im.rect(2, 4, 2, 17, WOOD); im.rect(2, 4, 1, 17, HEX(0xa8723d)); im.rect(3, 4, 1, 17, WOOD2);
    im.rect(22, 4, 2, 17, WOOD); im.rect(22, 4, 1, 17, HEX(0xa8723d)); im.rect(23, 4, 1, 17, WOOD2);
    im.rect(1, 3, 24, 2, WOOD); im.rect(1, 3, 24, 1, HEX(0xa8723d)); im.rect(1, 4, 24, 1, WOOD3);
    im.rect(1, 2, 24, 1, W); im.rect(3, 1, 6, 1, W2);  // snow on the bar
    // three fish hanging by the head (a round silver body, a forked tail, an eye) and a strip of dark smoked meat between them
    const int fcx[3] = {7, 13, 19}, flen[3] = {8, 10, 7};
    for (int i = 0; i < 3; i++) {
        const int cx = fcx[i], len = flen[i];
        im.set(cx, 5, HEX(0x3a3d46));                                                     // the cord
        im.ellipse(cx + 0.5f, 5.5f + len / 2.f, 2.6f, len / 2.f + 0.4f, HEX(0x94a6b8));  // the body
        im.ellipse(cx + 0.5f, 6.5f + len / 2.f, 1.5f, len / 2.f - 1.f, HEX(0xdfe8ef));   // the pale belly
        im.set(cx, 7, HEX(0x1a1d26));                                                     // the eye
        im.set(cx + 2, 9 + i % 2, HEX(0x5c6a7c));                                         // a fin
        const int ty = 6 + len;                                                           // the tail fork
        im.set(cx - 1, ty + 1, HEX(0x94a6b8)); im.set(cx + 2, ty + 1, HEX(0x94a6b8)); im.set(cx, ty, HEX(0x94a6b8)); im.set(cx + 1, ty, HEX(0x94a6b8));
    }
    im.rect(10, 6, 2, 9, HEX(0x6b2f2a)); im.rect(10, 6, 1, 9, HEX(0x8a4038));          // a strip of smoked meat
    im.rect(16, 6, 2, 7, HEX(0x6b2f2a)); im.rect(16, 6, 1, 7, HEX(0x8a4038));
    im.rect(1, 19, 24, 2, W); im.rect(3, 18, 6, 1, W); im.rect(16, 18, 7, 1, W2); im.rect(2, 20, 22, 1, W2);  // drifted round the posts
    im.outline(OUTL);
    return im;
}

// A stack of firewood (20 x 14, anchored at the middle of its foot): three rows of split logs, snow on top.
inline Img mkFirewood() {
    using namespace snowlm;
    Img im(20, 14);
    const u32 E = HEX(0xd8a868), B = HEX(0x6b4423), B2 = HEX(0x8d5e33), B3 = HEX(0x52341a);
    for (int row = 0; row < 3; row++) {
        const int n = 3 - row, y = 10 - row * 3, x0 = 10 - n * 3;
        for (int i = 0; i < n; i++) {
            const int x = x0 + i * 6;
            im.rect(x, y - 2, 6, 4, B); im.rect(x, y - 2, 6, 1, B2); im.rect(x, y + 1, 6, 1, B3);
            im.rect(x, y - 1, 2, 2, E); im.set(x + 1, y - 1, HEX(0xb8884a));  // the cut ends
        }
    }
    im.rect(3, 12, 14, 2, W); im.rect(8, 0, 4, 1, W); im.rect(7, 1, 6, 1, W2); im.rect(6, 2, 8, 1, W2);
    im.rect(2, 13, 16, 1, W2);
    im.outline(OUTL);
    return im;
}

// A tent that has come down (32 x 20, anchored like the standing one at its foot): the cloth sagging between two crooked poles, half buried.
inline Img mkTentOld() {
    using namespace snowlm;
    Img im(32, 22);
    const u32 CL = HEX(0xb07058), CD = HEX(0x8a5642), CK = HEX(0x6a3a2c), WOOD = HEX(0x8d5e33);
    for (int y = 8; y <= 18; y++) {
        const float t = (y - 8) / 10.f;
        const int hw = 4 + (int)(t * 11.f);
        for (int x = 16 - hw; x <= 16 + hw; x++) im.set(x, y + (x % 7 == 0 ? 1 : 0), x < 16 ? CL : CD);
    }
    im.line(16, 8, 24, 12, CK); im.line(16, 8, 8, 13, CK); im.line(12, 14, 20, 16, CK);  // creases where it folded
    im.line(7, 2, 13, 15, WOOD); im.line(8, 2, 14, 15, HEX(0x6b4423));                    // a pole slumped across
    im.line(25, 5, 21, 14, WOOD);                                                           // another, leaning
    im.rect(10, 13, 4, 3, HEX(0x2a1a1a));                                                   // a gap in the cloth
    im.rect(3, 15, 26, 4, W); im.rect(6, 12, 8, 3, W); im.rect(18, 11, 7, 3, W2); im.rect(9, 9, 5, 2, W); im.rect(4, 18, 24, 2, W2);  // snow over it
    im.outline(OUTL);
    return im;
}

// A cold fire ring (22 x 12): a ring of stones round a hearth of grey ash and a charred stub, a little snow in it.
inline Img mkAshRing() {
    using namespace snowlm;
    Img im(22, 12);
    im.ellipse(11.f, 6.5f, 10.f, 4.6f, HEX(0x7f8796));
    im.ellipse(11.f, 6.2f, 8.2f, 3.4f, HEX(0x3a3d46));
    im.ellipse(11.f, 6.4f, 6.8f, 2.6f, HEX(0x555a66));
    for (int i = 0; i < 12; i++) {  // the ring of stones
        const float a = i * 0.5236f;
        const int sx = 11 + (int)(cosf(a) * 9.f), sy = 6 + (int)(sinf(a) * 4.f);
        im.rect(sx - 1, sy - 1, 3, 2, i % 2 ? HEX(0xaab1be) : HEX(0x939aa9));
        im.set(sx - 1, sy - 1, HEX(0xdde3ea));
    }
    im.rect(8, 5, 5, 2, HEX(0x24201c)); im.rect(13, 6, 3, 1, HEX(0x24201c));  // charred sticks
    im.set(9, 4, W); im.set(15, 7, W); im.set(10, 8, W2);
    im.outline(OUTL);
    return im;
}

// The Ice Fisher's fishing hole (22 x 14, anchored at the middle of the hole): a round hole cut through the ice, a shovelled rim of snow,
// and a rod stub frozen in the rim.
inline Img mkFishHole() {
    using namespace snowlm;
    Img im(22, 14);
    im.ellipse(11.f, 7.5f, 10.4f, 5.8f, HEX(0xdbe6f2));  // the shovelled rim
    im.ellipse(11.f, 7.2f, 9.2f, 5.0f, HEX(0xf4f8fc));
    im.ellipse(11.f, 7.6f, 7.2f, 3.9f, HEX(0xaec3da));   // the cut edge of the ice
    im.ellipse(11.f, 7.8f, 6.2f, 3.2f, HEX(0x1c4a78));   // the water
    im.ellipse(11.f, 8.2f, 4.6f, 2.2f, HEX(0x123a62));
    im.set(8, 6, HEX(0x5aa0d0)); im.set(9, 5, HEX(0x7cc0e8)); im.set(13, 9, HEX(0x2f6aa0));
    im.line(0, 11, 2, 12, HEX(0xcfe4ee)); im.line(19, 2, 21, 3, HEX(0xcfe4ee));
    return im;
}
