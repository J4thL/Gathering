// appearance.hpp - how the hunter looks, as data: hairstyle, hair colour, skin tone and tunic colour. The salon changes these; the
// sprite generator (mkPlayer in sprites.hpp) reads them. Nothing here touches items, stats, skills or progression.
#pragma once
#include "gfx.hpp"

struct HunterLook {
    int hair = 0;    // hairstyle: index into HAIR_NAME
    int color = 0;   // hair colour: index into HAIRCOL
    int skin = 0;    // skin tone
    int outfit = 0;  // tunic colour
    bool operator==(const HunterLook& o) const { return hair == o.hair && color == o.color && skin == o.skin && outfit == o.outfit; }
    bool operator!=(const HunterLook& o) const { return !(*this == o); }
};
constexpr int LOOK_HAIR_N = 5, LOOK_COLOR_N = 8, LOOK_SKIN_N = 5, LOOK_OUTFIT_N = 6;
constexpr int SALON_FEE = 25;  // coins for a confirmed change

[[maybe_unused]] static const char* HAIR_NAME[LOOK_HAIR_N] = {"SHORT", "LONG", "PONYTAIL", "BRAIDS", "TOP BUN"};
[[maybe_unused]] static const char* HAIRCOL_NAME[LOOK_COLOR_N] = {"BROWN", "BLACK", "BLONDE", "RED", "CHESTNUT", "SILVER", "BLUE", "PINK"};
static const u32 HAIRCOL[LOOK_COLOR_N] = {HEX(0x5a3a22), HEX(0x24201e), HEX(0xd9b04a), HEX(0xa8321e), HEX(0x8a4a2a), HEX(0xc8ccd4), HEX(0x3a6ad0), HEX(0xe07aa8)};
[[maybe_unused]] static const char* SKIN_NAME[LOOK_SKIN_N] = {"FAIR", "LIGHT", "TAN", "BROWN", "DEEP"};
static const u32 SKINCOL[LOOK_SKIN_N] = {HEX(0xf0c49a), HEX(0xf8dcc0), HEX(0xd9a77a), HEX(0xa8704a), HEX(0x6e4630)};
static const u32 SKINSHADE[LOOK_SKIN_N] = {HEX(0xd9a77a), HEX(0xe0bf9f), HEX(0xb98a5e), HEX(0x8a5838), HEX(0x56362a)};
[[maybe_unused]] static const char* OUTFIT_NAME[LOOK_OUTFIT_N] = {"TAN", "GREEN", "BLUE", "RED", "PURPLE", "GREY"};
struct OutfitCol {
    u32 base, dark, hi;
};
static const OutfitCol OUTFITCOL[LOOK_OUTFIT_N] = {{HEX(0x9b6b3a), HEX(0x7a5229), HEX(0xb3834f)}, {HEX(0x4a7a46), HEX(0x365a34), HEX(0x6a9a64)},
                                                    {HEX(0x3a5a9a), HEX(0x2a437a), HEX(0x5a7aba)}, {HEX(0xa8403a), HEX(0x80302c), HEX(0xc8605a)},
                                                    {HEX(0x6a4a8a), HEX(0x503668), HEX(0x8a6aaa)}, {HEX(0x6a6a74), HEX(0x4e4e58), HEX(0x8a8a94)}};

inline HunterLook clampLook(HunterLook l) {
    auto c = [](int v, int n) { return v < 0 ? 0 : (v >= n ? n - 1 : v); };
    l.hair = c(l.hair, LOOK_HAIR_N);
    l.color = c(l.color, LOOK_COLOR_N);
    l.skin = c(l.skin, LOOK_SKIN_N);
    l.outfit = c(l.outfit, LOOK_OUTFIT_N);
    return l;
}
