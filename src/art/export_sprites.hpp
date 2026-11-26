// export_sprites.hpp - writes every drawn character / animal animation as PNG sprite sheets plus JSON metadata (run with --export-sprites DIR).
//
// The game draws all its art in code and does not read these files; they exist so the sprites can be inspected, edited in an art
// tool or used by other pipelines. Layout (under DIR):
//   sprites/player/<state>/player_<state>_<direction>.png           (gathering states: sprites/player/gathering/<kind>/...)
//   sprites/animals/<species>/<species>_<state>_<direction>.png
//   animations/player/<state>.json, animations/animals/<species>_<state>.json, animations/manifest.json
// A sheet is one row of frames (frame width x frame height each, in pose order). All four directions are written: the west-side
// ones are the mirrored east-side art, exactly as the game draws them. Common rarity of each species; a second form (stag, arctic wolf) is written under <species>_form1.
// PNGs are written uncompressed (stored deflate blocks) by a small encoder below, so there is no new dependency.
#pragma once
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include "art/sprites.hpp"

namespace sprexp {

inline uint32_t crc32(const uint8_t* d, size_t n, uint32_t c = 0) {
    static uint32_t T[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t v = i;
            for (int k = 0; k < 8; k++) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            T[i] = v;
        }
        init = true;
    }
    c = ~c;
    for (size_t i = 0; i < n; i++) c = T[(c ^ d[i]) & 255] ^ (c >> 8);
    return ~c;
}
inline void be32(std::vector<uint8_t>& v, uint32_t x) {
    for (int s = 24; s >= 0; s -= 8) v.push_back((uint8_t)(x >> s));
}
inline void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    be32(out, (uint32_t)data.size());
    std::vector<uint8_t> td(type, type + 4);
    td.insert(td.end(), data.begin(), data.end());
    out.insert(out.end(), td.begin(), td.end());
    be32(out, crc32(td.data(), td.size()));
}
// 8-bit RGBA PNG, no compression
inline bool writePng(const std::string& path, const Img& im) {
    std::vector<uint8_t> raw;
    raw.reserve((size_t)(im.w * 4 + 1) * im.h);
    for (int y = 0; y < im.h; y++) {
        raw.push_back(0);  // filter: none
        for (int x = 0; x < im.w; x++) {
            const u32 c = im.p[(size_t)y * im.w + x];
            raw.push_back((uint8_t)cR(c));
            raw.push_back((uint8_t)cG(c));
            raw.push_back((uint8_t)cB(c));
            raw.push_back((uint8_t)cA(c));
        }
    }
    std::vector<uint8_t> z = {0x78, 0x01};  // zlib header, no compression
    for (size_t pos = 0; pos < raw.size() || pos == 0;) {
        const size_t n = std::min<size_t>(65535, raw.size() - pos);
        const bool last = pos + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back((uint8_t)(n & 255));
        z.push_back((uint8_t)(n >> 8));
        z.push_back((uint8_t)(~n & 255));
        z.push_back((uint8_t)((~n >> 8) & 255));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
        if (last) break;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    be32(z, (b << 16) | a);
    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A}, ihdr;
    be32(ihdr, (uint32_t)im.w);
    be32(ihdr, (uint32_t)im.h);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});  // 8 bit, RGBA
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = fwrite(out.data(), 1, out.size(), f) == out.size();
    fclose(f);
    return ok;
}

inline const char* DIR_NAME[F_COUNT] = {"south", "east", "north", "west"};

inline std::string slug(const char* s) {
    std::string o;
    for (; *s; s++) o += (*s == ' ') ? '_' : (char)tolower(*s);
    return o;
}

// one row of frames
inline Img sheet(const std::vector<Img>& frames, bool mirror) {
    const int fw = frames[0].w, fh = frames[0].h;
    Img o(fw * (int)frames.size(), fh);
    for (size_t i = 0; i < frames.size(); i++) o.blit(mirror ? frames[i].flippedH() : frames[i], (int)i * fw, 0);
    return o;
}

struct Counter {
    int files = 0, failed = 0;
    std::string manifest;
};

// Writes the four directions (south, east, north, west) of one animation state and its metadata. `frames[view][pose]`, views in the game's order (S, E, N for a mirrored sprite; S, E, N, W for a rendered one).
inline void exportState(const std::string& root, const std::string& group, const std::string& spriteDir, const std::string& baseName, const std::string& metaName,
                        const std::vector<std::vector<Img>>& frames, const Clip& clip, int ax, int ay, Counter& cnt, bool direct = false, float disp = 1.f) {  // direct: frames[] holds all eight directions, none mirrored
    const int fw = frames[0][0].w, fh = frames[0][0].h;
    std::string files;
    for (int f = 0; f < F_COUNT; f++) {
        const FacingView fv = direct ? FacingView{f, false} : facingView(f);
        const std::string rel = "sprites/" + spriteDir + "/" + baseName + "_" + DIR_NAME[f] + ".png";
        if (writePng(root + "/" + rel, sheet(frames[fv.view], fv.mirror))) cnt.files++;
        else cnt.failed++;
        files += std::string(files.empty() ? "" : ",") + "\"" + DIR_NAME[f] + "\":\"" + rel + "\"";
    }
    std::string seq;
    char buf[96];
    for (int i = 0; i < clip.n; i++) {
        snprintf(buf, sizeof buf, "%s{\"frame\":%d,\"ms\":%d}", i ? "," : "", clip.frame[i], (int)lroundf(clip.dur[i] * 1000.f));
        seq += buf;
    }
    char head[256];
    snprintf(head, sizeof head, "{\n  \"name\":\"%s\",\n  \"group\":\"%s\",\n  \"frameWidth\":%d,\n  \"frameHeight\":%d,\n  \"frameCount\":%d,\n  \"loop\":%s,\n", baseName.c_str(),
             group.c_str(), fw, fh, (int)frames[0].size(), clip.loop ? "true" : "false");
    char anc[160];
    snprintf(anc, sizeof anc, "  \"anchor\":{\"x\":%d,\"y\":%d},\n  \"anchorMirrored\":{\"x\":%d,\"y\":%d},\n  \"displayScale\":%.4f,\n", ax, ay, fw - 1 - ax, ay, disp);
    const std::string json = std::string(head) + anc + "  \"orientation\":\"sheet is one row of frames, left to right\",\n  \"sequence\":[" + seq + "],\n  \"files\":{" + files + "}\n}\n";
    const std::string metaPath = root + "/animations/" + metaName + ".json";
    std::filesystem::create_directories(std::filesystem::path(metaPath).parent_path());
    if (FILE* f = fopen(metaPath.c_str(), "wb")) {
        fwrite(json.data(), 1, json.size(), f);
        fclose(f);
        cnt.files++;
    } else cnt.failed++;
    cnt.manifest += std::string(cnt.manifest.empty() ? "" : ",") + "\"animations/" + metaName + ".json\"";
}

// Returns the number of files written (PNGs + JSON); -1 when anything failed to write.
inline int exportAll(const std::string& root) {
    Counter cnt;
    // ---- the hunter (default appearance)
    static const int DIR_OF_VIEW[FACING_VIEWS] = {0, 2, 1};  // S, E, N -> mkPlayerPose dir
    static const char* HN[HS_COUNT] = {"idle", "walk", "run", "aim", "throw", "gather", "swing", "use", "hurt", "death", "mine", "forage"};
    static const char* HG[HS_COUNT] = {"idle", "walk", "run", "throw", "throw", "gathering/skinning", "gathering/chopping", "use", "hurt", "death", "gathering/mining", "gathering/harvesting"};
    for (int st = 0; st < HS_COUNT; st++) {
        std::vector<std::vector<Img>> fr(FACING_VIEWS);
        for (int v = 0; v < FACING_VIEWS; v++)
            for (int i = 0; i < hunterPoseCount(st); i++) {
                Img im = mkPlayerPose(DIR_OF_VIEW[v], hunterPose(st, i), HunterLook());
                styleActor(im, OUT, 0);
                fr[v].push_back(im);
            }
        const std::string grp = HG[st];
        exportState(root, "player", "player/" + grp, std::string("player_") + HN[st], std::string("player/") + HN[st], fr, hunterClip(st), 9, 24, cnt);
    }
    // ---- animals with a full animation set
    static const char* AN[AN_COUNT] = {"idle", "walk", "run", "attack", "hurt", "death", "fly", "takeoff", "land", "eat", "rest", "alert"};
    for (int kind : RICH_KINDS)
        for (int form = 0; form < ANIMAL[kind].forms; form++) {  // form 0 keeps the plain name, a second form (stag, arctic wolf ...) gets _form1
            const AnimalClips& C = *animalClips(kind);
            const std::string sp = slug(ANIMAL[kind].name) + (form ? "_form" + std::to_string(form) : "");
            const RigShape R = rigShape(kind, form);
            const QuadSpec spec = fauSpec(kind, form, 0);
            const float k = TIER[0].scale * rigScale(R);
            for (int st = 0; st < AN_COUNT; st++) {
                if (C.poses[st] <= 0) continue;
                std::vector<std::vector<Img>> fr(AV_VIEWS);
                for (int v = 0; v < AV_VIEWS; v++)
                    for (int i = 0; i < C.poses[st]; i++) {
                        Img im = paintQuad(spec, R, TIER[0].scale, v, R.bird ? birdPose(st, i) : (R.hopper ? hopPose(st, i) : quadPose(st, i)));
                        styleActor(im, OUT, R.flat ? 2 : 0);
                        fr[v].push_back(im);
                    }
                exportState(root, "animal", "animals/" + sp, sp + "_" + AN[st], "animals/" + sp + "_" + AN[st], fr, C.clip[st], fr[0][0].w / 2,
                            (int)(fr[0][0].h - 6.f * k), cnt, true, 1.f);
            }
        }
    const std::string mp = root + "/animations/manifest.json";
    if (FILE* f = fopen(mp.c_str(), "wb")) {
        const std::string s = "{\n  \"files\":[" + cnt.manifest + "]\n}\n";
        fwrite(s.data(), 1, s.size(), f);
        fclose(f);
        cnt.files++;
    } else cnt.failed++;
    return cnt.failed ? -1 : cnt.files;
}

}  // namespace sprexp
