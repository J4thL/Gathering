// sprite_style.hpp - the shared finishing pass for characters and animals (the "reference look": coloured contours instead of black,
// a top rim light and underside shading for volume, and an optional white "sticker" outline on animals so they read against the ground).
// It works on the finished Img, using the universal outline colour OUT as the marker for the contour, so every existing sprite
// painter (animals of all body plans, the hunter) gets the same look without being redrawn.
#pragma once
#include "core/gfx.hpp"

constexpr u32 ANIMAL_RIM = 0xfff4efe6;  // warm white

// Richer fur: a little more saturation and contrast on every drawn pixel (the white rim and the transparent pixels are left alone), so the animals hold their
// own against the lime grass. Snow whites stay white; shadows get a touch deeper.
inline void richenActor(Img& im, u32 outCol, float sat = 1.22f, float contrast = 1.08f) {  // outCol: the contour marker, kept as it is for styleActor
    for (auto& c : im.p) {
        if (!cA(c) || c == ANIMAL_RIM || c == outCol) continue;
        const float r = cR(c) / 255.f, g = cG(c) / 255.f, b = cB(c) / 255.f;
        const float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b)), l = (mx + mn) * 0.5f;
        auto ch = [&](float v) { return std::max(0.f, std::min(1.f, ((l + (v - l) * sat) - 0.5f) * contrast + 0.5f)); };
        c = RGBA((int)(ch(r) * 255.f), (int)(ch(g) * 255.f), (int)(ch(b) * 255.f), cA(c));
    }
}

// mode 0: contour = darkened colour of what it surrounds (characters); mode 1: contour = white rim with a dark inner line (animals);
// mode 2: no contour (the flat reference look)
inline void styleActor(Img& im, u32 outCol, int mode) {
    if (mode == 2) {  // flat pixel-art finish: no contour at all - the shapes and the colour contrast carry the silhouette
        for (auto& c : im.p)
            if (c == outCol) c = 0u;
        return;
    }
    auto isOut = [&](int x, int y) { return im.in(x, y) && im.get(x, y) == outCol; };
    auto isFill = [&](int x, int y) { return im.in(x, y) && cA(im.get(x, y)) && im.get(x, y) != outCol; };
    Img o = im;
    for (int y = 0; y < im.h; y++)
        for (int x = 0; x < im.w; x++) {
            if (isOut(x, y)) {
                if (mode == 1) {  // a rim only on the outside: a contour squeezed between two parts of the body (the gap between legs) stays dark
                    const bool squeezed = (isFill(x - 1, y) && isFill(x + 1, y)) || (isFill(x, y - 1) && isFill(x, y + 1));
                    if (!squeezed) o.set(x, y, ANIMAL_RIM);
                    continue;
                }
                int r = 0, g = 0, b = 0, n = 0;
                for (int d = 0; d < 4; d++) {
                    const int nx = x + (d == 0) - (d == 1), ny = y + (d == 2) - (d == 3);
                    if (isFill(nx, ny)) {
                        const u32 c = im.get(nx, ny);
                        r += cR(c), g += cG(c), b += cB(c), n++;
                    }
                }
                if (n) o.set(x, y, RGBA(r / n * 40 / 100, g / n * 38 / 100, b / n * 46 / 100));  // dark, slightly cool
                continue;
            }
            if (!isFill(x, y)) continue;
            const u32 c = im.get(x, y);
            const bool edgeUp = !isFill(x, y - 1) && !isOut(x, y - 1) ? true : isOut(x, y - 1);
            const bool edgeDown = isOut(x, y + 1) || !cA(im.get(x, y + 1));
            const bool edgeSide = isOut(x - 1, y) || isOut(x + 1, y);
            u32 v = c;
            if (mode == 1 && (edgeUp || edgeDown || edgeSide)) v = shadeC(c, 0.72f);  // the dark inner contour inside the white rim
            else if (edgeUp) v = shadeC(c, 1.14f);                                    // rim light from above
            else if (edgeDown) v = shadeC(c, 0.84f);                                  // underside
            o.set(x, y, v);
        }
    im = o;
}
