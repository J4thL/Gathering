// tiles_user.hpp - the water and shore-bank tile art, hand-drawn by the game's author and embedded as palette-indexed rows
// (one character per pixel, index into TILE_PAL). WATER is a
// 64 x 16 band that repeats horizontally (scrolled to animate the flow); BANK is the 64 x 16 dirt bank and stone ledge drawn on a water
// tile that has land to its north.
#pragma once
#include "core/gfx.hpp"

static const unsigned TILE_PAL[43] = {0x538a25, 0x70a82e, 0x92cc4e, 0x2b6f0a, 0x28650b, 0x458127, 0x7cb935, 0x376b1e, 0x4ea026, 0x477730, 0x3978a8, 0x7c9baf, 0x6d93ad, 0x28ccdf, 0xdff6f5, 0xbf7958, 0x7a444a, 0xa05b53, 0xeea160, 0x71aa34, 0x397b44, 0xb6d53c, 0x5e3643, 0xcfc6b8, 0xa0938e, 0x4f6586, 0x78868e, 0x67788b, 0xbeb9b1, 0x5c6d89, 0xbcb7b0, 0xc2bcb2, 0xc0bab1, 0x387884, 0x4c897e, 0x649881, 0x648f89, 0x89a894, 0x7a9e92, 0x6d9396, 0x96ad95, 0x78a083, 0x90af86};
constexpr int WATER_TEX_W = 64, WATER_TEX_H = 16, BANK_TEX_W = 64, BANK_TEX_H = 16;
static const char* const WATER_ROWS[16] = {
    "abbbbbbaaaaaaaaaaabbbbbaabaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "aaaaaaaaaaaaaaaabbaaaaabbaaaaaaaaaaaaaaaaaaaaaaabaaabbbbbaaaaaaa",
    "acccccacacaaaaaaaaaaaaaaaaaaaaaaaaaaccccccaaaaaaabbbaaaaabaaaaaa",
    "aaaaaaaaaaaaaaaaaaaaaaaaacccccaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "aadadddeeddeadaeaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbba",
    "aaaaaaaaaaaaaaaaaaaaaaabbaaaaaaaaadaeadeedddedaeadaaaaaaaaabaaaa",
    "aaaaaaaaaaaaaaaaaaaaaabaabbbbbaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "acacccccccacacaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaacaccccccaacaaaaaaaaaaaaaaaaaa",
    "aaaaaaadaeaeddeaaeaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "aaaaabaaaaaaaaaaaaaaaaaacccccccaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "bbbbbaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbaabaaaaaeaeeddeadaaaa",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabaaaaaaaaaaaaaaaaaaaa"};
static const char* const BANK_ROWS[16] = {
    "fghhhijjhfffikfffiigkhhffjfhhlgffjhffffhgfffjfghhiihkjihhjijffff",
    "fhhiiffjfifiihhfifffihffifihgffffhhgiiffgffiifghiffhfffiijfjfigm",
    "fhfffiikfffffgghhifffhgfffghhfffgfihgffifgffffgifighgmfffhgfiimm",
    "ghfffffmmmmgghhghhggghhmmghifgmgfffihmmmghgggmhggmhhhhiifhgffmhg",
    "gghgggghhnnoonhfiifoneenmhffimonneoonnoehgmmmhhfiimhhhmfihhmmhhm",
    "iihmononepppppgfifhpppppohgmmnppppppppppnohhhgffffmnooennoonnnoe",
    "fimnpppqrpsssthmffotuuutpnenoptsttuvwwtpppoeeohmmoopppppppppptqp",
    "fmnprusrqqttttoemmntpptpppptqrwrppprqtptstttppnnepppppuupwwwwttq",
    "meddeeetetdptpqtnetrrrprqwwusqrprwrpdppededtdrqppppwwrrrxqqrrrtr",
    "epqryryqqrrrtrrqtprqqrrprrqqpttrrrrptpqqrrtqrrwvwttqqrqryrrqqtsw",
    "rwrryqyttqyxtqqttpryrrrptrrrrtyrrpzttryrrttxpqqrrrxqrrrtyyqtttpr",
    "wqqtpyyzybbbbbtttqyeydydededeeyydzzytqyrpttxtttpqryddxdeedxdxyqr",
    "pptpxxyybyyyyxxxptxrzqqttxtttttyxxpztyAbbbbyypttttyyxtytyzyzyxtq",
    "yxyyyyyzyzyzzyyyyyyxxpptpxyxttpxyyyzxxyypyybyyypyyzyyyttpyyyyzxp",
    "zyxyBCByzyBzyzyzyzyCCCCDDByyzyyyyyzyyzzyyyzyzyyyzzyzzBCCCEzByyyy",
    "azyyayyazzyyzayyzazyyzazyyzyyzycBCCBCaaByzBzayyzaazyzzyzzzFGGFFa"};

inline int tilePalIndex(char c) { return c <= '9' ? c - '0' : (c >= 'a' ? 10 + c - 'a' : 36 + c - 'A'); }
// One tw x th crop of a repeating texture, starting at (x0, y0) and wrapping at the edges. `shade` darkens it (dark grass).
inline Img tileCrop(const char* const* rows, int texW, int texH, int x0, int y0, int tw, int th, float shade = 1.f) {
    Img im(tw, th);
    for (int y = 0; y < th; y++)
        for (int x = 0; x < tw; x++) {
            const unsigned c = TILE_PAL[tilePalIndex(rows[(y0 + y) % texH][(x0 + x) % texW])];
            const float tr = shade < 1.f ? 0.96f : 1.f, tb = shade < 1.f ? 0.92f : 1.f;  // dark grass also drifts a little toward green
            const int r = (int)(((c >> 16) & 255) * shade * tr), g = (int)(((c >> 8) & 255) * shade), b = (int)((c & 255) * shade * tb);
            im.set(x, y, HEX((u32)((r << 16) | (g << 8) | b)));
        }
    return im;
}
