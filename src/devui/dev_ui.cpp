// dev_ui.cpp - the developer panel (Dear ImGui, F1). Only compiled into builds with GATHERING_DEV_UI (see dev_ui.hpp).
//
//   WORLD  map, seed, the worldgen sliders (world.hpp: WorldTune) and a regenerate button, so terrain can be tuned by eye
//   MAP    a preview of the whole active map (tiles, props, forest density, solid / light-blocking tiles); click to teleport
//   TIME   time of day and day number, with a freeze
//   SPAWN  give items and coins, spawn animals (any species and rarity) and, inside a delve, monsters; heal, god mode
//   DEBUG  frame rate, counters, the AI overlay (F3), window screenshot
//
// The panel draws straight onto the window, on top of the game's scaled canvas, so it is crisp at any window size and never touches the
// game's own pixel-art rendering. The game does not see mouse or keyboard input while the panel wants it.
#ifdef GATHERING_DEV_UI
#include "devui/dev_ui.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include "core/game.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

namespace devui {
namespace {

bool g_inited = false, g_open = false;
bool g_swallowLeft = false;     // a left-button press went to the panel, so its release must not reach the game either
uint32_t g_seed = 0;            // the seed box
bool g_seedTouched = false;     // the panel changed Game::worldSeed (it is put back at exit so the save keeps its own world)
uint32_t g_seedBefore = 0, g_seedSet = 0;
bool g_auto = true, g_dirty = false;  // regenerate by itself when a slider is let go
bool g_freeze = false;
float g_freezeT = 0;
int g_layer = 0;
float g_mapScale = 1.5f;
SDL_Texture* g_tex = nullptr;
std::vector<u32> g_pix;
std::string g_note;             // last thing the panel did, shown at the bottom
const char* g_shotPath = nullptr;
bool g_god = false, g_infSta = false;  // held while the panel is open
char g_itemFilter[48] = "";
int g_itemSel = -1, g_giveN = 1;
int g_spKind = 0, g_spTier = 0, g_spForm = 0, g_spN = 1, g_spMon = 0;
bool g_spElite = false;
float g_spDepth = 0.f;
int g_coins = 100;

uint32_t tileHash(const World& w) {  // FNV-1a over every tile id: two runs that print the same number generated the same ground
    uint32_t h = 2166136261u;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) h = (h ^ w.tile[y][x]) * 16777619u;
    return h;
}

bool tuneIsDefault(const WorldTune& t) {
    const WorldTune d;
    return std::memcmp(&t, &d, sizeof(WorldTune)) == 0;
}

// Generate the active map again with the current seed and tuning. The outside is kept as it is (structures, saved state); only what lives on
// the old ground is dropped, and the hunter is moved to the nearest standable tile if the ground changed under their feet.
void regenerate(Game& g) {
    if (g.world.isInterior() || g.world.mapId >= B_COUNT) { g_note = "nothing to regenerate inside a building"; return; }
    const int map = g.world.mapId;
    if (g.worldSeed != g_seed) {
        if (!g_seedTouched) g_seedBefore = g.worldSeed;
        g_seedTouched = true;
        g.worldSeed = g_seed;
        g_seedSet = g_seed;
    }
    g.animals.clear();
    g.projs.clear();
    g.parts.clear();
    g.ftexts.clear();
    g.decals.clear();
    g.items.clear();
    g.loadMapWorld(map);
    if (map == B_HOME) g.populate();
    g.p.pos = g.freeSpot((int)(g.p.pos.x / TS), (int)(g.p.pos.y / TS));
    g.hvKey = -1;
    g.setCam(0, true);
    g_dirty = false;
    char b[96];
    snprintf(b, sizeof b, "regenerated %s, seed %u", BIOME[map].name, (unsigned)g.worldSeed);
    g_note = b;
}

bool slider(const char* label, float* v, float lo, float hi, const char* fmt, const char* tip = nullptr) {
    const bool ch = ImGui::SliderFloat(label, v, lo, hi, fmt);
    if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    if (ch) g_dirty = true;
    return ch;
}

// ---------------------------------------------------------------- WORLD
void tabWorld(Game& g) {
    World& w = g.world;
    const int map = w.mapId;
    const bool outdoors = map >= 0 && map < B_COUNT;
    ImGui::Text("Map: %s", outdoors ? BIOME[map].name : "(inside)");
    ImGui::SameLine();
    ImGui::TextDisabled("  tile hash %08x", (unsigned)tileHash(w));
    if (g.state == ST_PLAY) {  // jumping needs a running game (it uses the portal code)
        for (int b = B_HOME; b <= B_MOUNTAIN; b++) {
            if (b != B_HOME) ImGui::SameLine();
            ImGui::PushID(b);
            if (ImGui::SmallButton(BIOME[b].name) && b != map) {
                if (g.travelTo(b, MAPS[b].entryTx, MAPS[b].entryTy, false)) g_note = std::string("travelled to ") + BIOME[b].name;
            }
            ImGui::PopID();
        }
    }
    ImGui::Separator();

    ImGui::SetNextItemWidth(130);
    ImGui::InputScalar("seed", ImGuiDataType_U32, &g_seed, nullptr, nullptr, "%u");
    ImGui::SameLine();
    if (ImGui::Button("random")) g_seed = (uint32_t)(SDL_GetPerformanceCounter() * 2654435761u) % 1000000u;
    ImGui::SameLine();
    if (ImGui::SmallButton("current")) g_seed = g.worldSeed;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("the seed this game was started with (%u)", (unsigned)g.worldSeed);

    ImGui::BeginDisabled(!outdoors);
    if (ImGui::Button("Regenerate map")) regenerate(g);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("when a slider is let go", &g_auto);
    if (g_dirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.f, 0.8f, 0.3f, 1.f), "*");
    }
    if (g_auto && g_dirty && !ImGui::IsAnyItemActive() && outdoors) regenerate(g);

    WorldTune& t = w.tune;
    ImGui::SeparatorText("Snowfields");
    ImGui::BeginDisabled(map != B_SNOW);
    bool sv2 = (w.genOff & GEN_V2_SNOW) != 0;
    if (ImGui::Checkbox("v2 terrain (FastNoiseLite)##snow", &sv2)) {
        w.genOff = sv2 ? (w.genOff | GEN_V2_SNOW) : (w.genOff & ~(uint32_t)GEN_V2_SNOW);
        g_dirty = true;
    }
    ImGui::SliderFloat("ground detail", &t.snowDetail, 0.f, 1.f, "%.2f");  // drawn only: no regeneration
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("wind-carved drifts and ripples over the snow (needs the v2 terrain; 0 = off)");
    bool seeded = (w.genOff & GEN_FIXED_LANDMARKS) == 0;
    if (ImGui::Checkbox("seeded landmark places##snow", &seeded)) {
        w.genOff = seeded ? (w.genOff & ~(uint32_t)GEN_FIXED_LANDMARKS) : (w.genOff | GEN_FIXED_LANDMARKS);
        g_dirty = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("the Watch Cairn and the Frozen Fortress stand where the seed puts them (off = the old fixed places)");
    ImGui::BeginDisabled(!sv2);
    bool spl = (w.genOff & GEN_V2_PLATEAU) != 0;
    if (ImGui::Checkbox("plateaus (rock face on the south edge)", &spl)) {
        w.genOff = spl ? (w.genOff | GEN_V2_PLATEAU) : (w.genOff & ~(uint32_t)GEN_V2_PLATEAU);
        g_dirty = true;
    }
    ImGui::BeginDisabled(!spl);
    slider("plateau size", &t.sPlatFreq, 0.015f, 0.08f, "%.3f", "lower = bigger plateaus");
    slider("plateau cover", &t.sPlatCut, 0.45f, 0.80f, "%.2f", "the cut on the shape field: higher = fewer plateaus");
    slider("plateau zoning", &t.sPlatZone, 0.f, 0.4f, "%.2f", "0 = an even scatter; higher = massifs in the outcrop zones, open ground elsewhere");
    slider("massif scale", &t.sPlatBig, 0.1f, 1.0f, "%.2f", "the shape field in the outcrops, as a fraction of the normal frequency: lower = bigger massifs");
    slider("plateau wander", &t.sPlatWarp, 0.f, 24.f, "%.1f", "how far the outlines wander, in tiles");
    ImGui::EndDisabled();
    slider("zone warp", &t.sZoneWarp, 0.f, 30.f, "%.1f", "how far the forest / outcrop / slope zones are bent, in tiles");
    slider("drift warp", &t.sDriftWarp, 0.f, 40.f, "%.1f", "how far the wind-drift streaks are bent");
    slider("ridge warp", &t.sRidgeWarp, 0.f, 20.f, "%.1f", "how much the ridge lines curve");
    slider("ridge contrast", &t.sRidgeK, 0.15f, 1.2f, "%.2f", "higher = thinner, shorter ridges at the same width");
    slider("ice wander", &t.sLakeAmp, 0.f, 6.f, "%.1f", "frozen-lake outline wander, in tiles");
    slider("ridge ends", &t.sRidgeMinGrad, 0.f, 0.04f, "%.3f", "ridges end where their field is flat (saddles, peaks): higher = more, shorter ridges, fewer blobs and crossings");
    ImGui::EndDisabled();
    slider("ridge width", &t.snowRidgeW, 0.f, 0.06f, "%.3f", "half-width of the snow-capped rock ridges (0 = none)");
    slider("ridge gap", &t.snowRidgeGap, 0.3f, 1.0f, "%.2f", "ridges are broken wherever the gap noise is below this: higher = longer, fewer breaks");
    slider("packed snow", &t.snowPack, 0.45f, 0.80f, "%.2f", "packed-snow threshold: lower = more packed snow");
    ImGui::EndDisabled();
    if (map != B_SNOW) ImGui::TextDisabled("(go to the snowfields to see these)");

    ImGui::SeparatorText("Home forest");
    ImGui::BeginDisabled(map != B_HOME);
    bool v2 = (w.genOff & GEN_V2) != 0;
    if (ImGui::Checkbox("v2 terrain (FastNoiseLite)", &v2)) {
        w.genOff = v2 ? (w.genOff | GEN_V2) : (w.genOff & ~(uint32_t)GEN_V2);
        g_dirty = true;
    }
    ImGui::BeginDisabled(!v2);
    slider("thicket freq", &t.fDensFreq, 0.02f, 0.12f, "%.3f", "size of the thickets and clearings (lower = bigger)");
    slider("thicket warp", &t.fDensWarp, 0.f, 40.f, "%.1f", "how far the thicket field is bent, in tiles (0 = plain noise)");
    slider("stand freq", &t.fStandFreq, 0.02f, 0.12f, "%.3f", "size of the moss / needle / leaf-litter stands");
    slider("stand warp", &t.fStandWarp, 0.f, 24.f, "%.1f", "how ragged the stands' borders are");
    slider("edge freq", &t.fEdgeFreq, 0.02f, 0.20f, "%.3f", "how fast glade, field and lake outlines wander");
    slider("glade wander", &t.fGladeAmp, 0.f, 0.6f, "%.2f");
    slider("field wander", &t.fFieldAmp, 0.f, 0.6f, "%.2f");
    slider("lake wander", &t.fLakeAmp, 0.f, 6.f, "%.1f", "tiles");
    slider("brook meander", &t.fBrookAmp, 0.f, 4.f, "%.1f");
    ImGui::EndDisabled();
    ImGui::TextDisabled("switch off:");
    struct Sw { uint32_t bit; const char* name; } sw[] = {{GEN_NO_STREAMS, "brooks"}, {GEN_NO_GLADES, "glades"}, {GEN_NO_SMOOTH, "smoothing"}, {GEN_NO_FIELDS, "flower fields"}};
    for (auto& s : sw) {
        ImGui::SameLine();
        bool on = (w.genOff & s.bit) != 0;
        if (ImGui::Checkbox(s.name, &on)) {
            w.genOff = on ? (w.genOff | s.bit) : (w.genOff & ~s.bit);
            g_dirty = true;
        }
    }
    ImGui::EndDisabled();
    if (map != B_HOME) ImGui::TextDisabled("(go to the wildwood to see these)");

    ImGui::Separator();
    if (ImGui::Button("Reset sliders")) {
        w.tune = WorldTune();
        w.genOff = GEN_DEFAULT;
        g_dirty = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled(tuneIsDefault(t) ? "sliders at the shipped values" : "sliders changed");
}

// ---------------------------------------------------------------- MAP
void buildPreview(Game& g) {
    const World& w = g.world;
    if (!g_tex) {
        g_tex = SDL_CreateTexture(g.ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, MW, MH);
        g_pix.assign((size_t)MW * MH, 0u);
    }
    if (!g_tex) return;
    if (g_layer == 0 || g_layer == 1) {
        const Img im = w.tileMap(g_layer == 0);
        std::copy(im.p.begin(), im.p.end(), g_pix.begin());
    } else {
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) {
                u32 c = HEX(0x20242c);
                if (g_layer == 2) {
                    if (!w.homeDens.empty()) {  // sand (open country) -> dark green (dense forest); the 0.5 line is where trees thin out
                        const float v = std::min(1.f, std::max(0.f, (w.homeDens[(size_t)y * MW + x] - 0.25f) / 0.5f));
                        c = RGBA((int)(206 - 160 * v), (int)(186 - 80 * v), (int)(120 - 70 * v));
                        if (w.tile[y][x] == T_WATER || w.tile[y][x] == T_LAKE_MID || w.tile[y][x] == T_LAKE_SHALLOW) c = HEX(0x2b6fd0);
                    }
                } else {
                    c = w.solid[y][x] ? HEX(0xd8584a) : HEX(0x2f3a34);
                    if (w.occ[y][x]) c = w.solid[y][x] ? HEX(0xf0a030) : HEX(0x5a78d8);
                }
                g_pix[(size_t)y * MW + x] = c;
            }
    }
    SDL_UpdateTexture(g_tex, nullptr, g_pix.data(), MW * 4);
}

void tabMap(Game& g) {
    if (g.world.isInterior()) { ImGui::TextDisabled("no map preview inside a building"); return; }
    ImGui::SetNextItemWidth(170);
    ImGui::Combo("layer", &g_layer, "tiles + props\0tiles\0forest density (home)\0solid / light-blocking\0");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    ImGui::SliderFloat("zoom", &g_mapScale, 1.f, 3.f, "%.1fx");
    buildPreview(g);
    if (!g_tex) return;
    const ImVec2 size(MW * g_mapScale, MH * g_mapScale);
    ImGui::Image((ImTextureID)g_tex, size);
    const ImVec2 p0 = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float px = p0.x + g.p.pos.x / TS * g_mapScale, py = p0.y + g.p.pos.y / TS * g_mapScale;  // the hunter
    dl->AddCircleFilled(ImVec2(px, py), 3.5f, IM_COL32(255, 60, 60, 255));
    dl->AddCircle(ImVec2(px, py), 5.5f, IM_COL32(255, 255, 255, 255), 0, 1.5f);
    if (ImGui::IsItemHovered()) {
        const ImVec2 m = ImGui::GetMousePos();
        const int tx = (int)((m.x - p0.x) / g_mapScale), ty = (int)((m.y - p0.y) / g_mapScale);
        if (tx >= 0 && ty >= 0 && tx < MW && ty < MH) {
            ImGui::SetTooltip("tile %d, %d   (click to go there)\nground %d%s", tx, ty, g.world.tile[ty][tx], g.world.solid[ty][tx] ? "   solid" : "");
            if (ImGui::IsMouseClicked(0)) {
                g.p.pos = g.freeSpot(tx, ty);
                g.p.kb = V2();
                g.setCam(0, true);
                char b[64];
                snprintf(b, sizeof b, "teleported to tile %d, %d", tx, ty);
                g_note = b;
            }
        }
    }
    ImGui::TextDisabled("red dot = you; click the map to teleport");
}

// ---------------------------------------------------------------- TIME
void tabTime(Game& g) {
    const float tod = g.tod();
    float hour = std::fmod(tod * 24.f + 6.f, 24.f);  // tod 0.25 is noon (Game::daylight peaks there)
    const int day = (int)(g.dayT / DAY_LEN);
    ImGui::Text("day %d   season %d of %d   daylight %.2f", day + 1, g.season() + 1, (int)SEASON_COUNT, g.daylight());
    auto setTod = [&](float h) {
        const float t = std::fmod(std::fmod(h - 6.f, 24.f) + 24.f, 24.f) / 24.f;
        const float fr = std::fmod(t - 0.05f + 1.f, 1.f);
        g.dayT = std::floor(g.dayT / DAY_LEN) * DAY_LEN + fr * DAY_LEN;
        g_freezeT = g.dayT;
    };
    if (ImGui::SliderFloat("hour", &hour, 0.f, 24.f, "%.1f h")) setTod(hour);
    if (ImGui::Button("dawn")) setTod(6.f);
    ImGui::SameLine();
    if (ImGui::Button("noon")) setTod(12.f);
    ImGui::SameLine();
    if (ImGui::Button("dusk")) setTod(18.f);
    ImGui::SameLine();
    if (ImGui::Button("midnight")) setTod(0.f);
    if (ImGui::Checkbox("freeze time (while this panel is open)", &g_freeze)) g_freezeT = g.dayT;
    int d = day + 1;
    ImGui::SetNextItemWidth(110);
    if (ImGui::InputInt("day", &d, 1, 7) && d >= 1) {
        const float fr = g.dayT / DAY_LEN - std::floor(g.dayT / DAY_LEN);
        g.dayT = ((float)(d - 1) + fr) * DAY_LEN;
        g_freezeT = g.dayT;
    }
    ImGui::TextDisabled("weather follows the day count, so changing the day changes the weather");
}


// ---------------------------------------------------------------- SPAWN
bool containsNoCase(const std::string& hay, const char* needle) {
    if (!*needle) return true;
    std::string h = hay, n = needle;
    for (auto& c : h) c = (char)tolower((unsigned char)c);
    for (auto& c : n) c = (char)tolower((unsigned char)c);
    return h.find(n) != std::string::npos;
}

V2 spawnSpot(Game& g, int i) {  // a few tiles in front of the hunter, fanned out so a group does not stack, on the nearest standable tile
    const float a = (float)i * 0.9f;
    const V2 at = g.p.pos + V2(40.f + 10.f * (float)(i / 6), 0.f) + V2(sinf(a), cosf(a)) * 24.f * (float)(i % 6 ? 1 : 0);
    return g.freeSpot((int)(at.x / TS), (int)(at.y / TS));
}

void tabSpawn(Game& g) {
    ImGui::SeparatorText("Give");
    ImGui::SetNextItemWidth(170);
    ImGui::InputTextWithHint("##filter", "filter items...", g_itemFilter, sizeof g_itemFilter);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::InputInt("##n", &g_giveN, 1, 10);
    g_giveN = std::max(1, std::min(999, g_giveN));
    ImGui::BeginChild("items", ImVec2(0, 150), true);
    for (int id = 0; id < ITEM_COUNT; id++) {
        const ItemDef& D = item(id);
        if (D.name.empty() || !containsNoCase(D.name, g_itemFilter)) continue;
        char row[96];
        snprintf(row, sizeof row, "%s##%d", D.name.c_str(), id);
        if (ImGui::Selectable(row, g_itemSel == id, ImGuiSelectableFlags_AllowDoubleClick)) {
            g_itemSel = id;
            if (ImGui::IsMouseDoubleClicked(0)) {
                const int got = g.p.inv.add(id, g_giveN);
                char b[96];
                snprintf(b, sizeof b, "gave %d x %s%s", got, D.name.c_str(), got < g_giveN ? " (pack full or too heavy)" : "");
                g_note = b;
            }
        }
    }
    ImGui::EndChild();
    ImGui::BeginDisabled(!itemValid(g_itemSel));
    if (ImGui::Button("Give selected")) {
        const int got = g.p.inv.add(g_itemSel, g_giveN);
        char b[96];
        snprintf(b, sizeof b, "gave %d x %s%s", got, item(g_itemSel).name.c_str(), got < g_giveN ? " (pack full or too heavy)" : "");
        g_note = b;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("(or double-click an item)");

    ImGui::SeparatorText("Hunter");
    ImGui::SetNextItemWidth(90);
    ImGui::InputInt("##coins", &g_coins, 50, 500);
    ImGui::SameLine();
    if (ImGui::Button("add coins")) g.p.coins = std::max(0, g.p.coins + g_coins);
    ImGui::SameLine();
    if (ImGui::Button("heal")) { g.p.hp = g.p.maxhp; g.p.stamina = 100.f; }
    ImGui::Checkbox("god mode", &g_god);
    ImGui::SameLine();
    ImGui::Checkbox("endless stamina", &g_infSta);
    ImGui::SameLine();
    ImGui::TextDisabled("(while the panel is open)");

    if (g.world.mapId < B_COUNT) {
        ImGui::SeparatorText("Animals");
        const char* an[A_COUNT];
        for (int k = 0; k < A_COUNT; k++) an[k] = ANIMAL[k].name;
        ImGui::SetNextItemWidth(150);
        ImGui::Combo("species", &g_spKind, an, A_COUNT, 12);
        const char* tn[TR_COUNT];
        for (int t = 0; t < TR_COUNT; t++) tn[t] = TIER[t].name;
        ImGui::SetNextItemWidth(110);
        ImGui::Combo("rarity", &g_spTier, tn, TR_COUNT);
        const int forms = ANIMAL[g_spKind].forms;
        g_spForm = std::min(g_spForm, forms - 1);
        if (forms > 1) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            ImGui::SliderInt("form", &g_spForm, 0, forms - 1);
        }
        ImGui::SetNextItemWidth(110);
        ImGui::SliderInt("count", &g_spN, 1, 12);
        ImGui::SameLine();
        if (ImGui::Button("Spawn near me")) {
            int made = 0;
            for (int i = 0; i < g_spN; i++)
                if (g.spawnAnimal(g_spKind, spawnSpot(g, i), g_spForm, g_spTier)) made++;
            char b[96];
            snprintf(b, sizeof b, "spawned %d %s (%s)", made, ANIMAL[g_spKind].name, TIER[g_spTier].name);
            g_note = b;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear all animals")) { g.animals.clear(); g_note = "all animals removed (the world re-populates over time)"; }
    } else {
        ImGui::SeparatorText("Monsters (delve)");
        const char* mn[MK_COUNT];
        for (int k = 0; k < MK_COUNT; k++) mn[k] = MONDEF(k).name;
        ImGui::SetNextItemWidth(150);
        ImGui::Combo("monster", &g_spMon, mn, MK_COUNT, 12);
        ImGui::SetNextItemWidth(110);
        ImGui::SliderFloat("depth", &g_spDepth, 0.f, 1.f, "%.2f");
        ImGui::SameLine();
        ImGui::Checkbox("elite", &g_spElite);
        ImGui::SetNextItemWidth(110);
        ImGui::SliderInt("count", &g_spN, 1, 12);
        ImGui::SameLine();
        if (ImGui::Button("Spawn near me")) {
            for (int i = 0; i < g_spN; i++) g.spawnMonster(g_spMon, spawnSpot(g, i), g_spDepth, g_spElite, -1);
            g_note = std::string("spawned ") + MONDEF(g_spMon).name;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear monsters")) { g.mons.clear(); g_note = "monsters removed"; }
    }
}

// ---------------------------------------------------------------- DEBUG
void saveWindow(Game& g, const char* path) {
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(g.ren, &w, &h);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return;
    SDL_SetRenderTarget(g.ren, nullptr);
    if (SDL_RenderReadPixels(g.ren, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0) SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
}

void tabDebug(Game& g) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("%.0f fps   %.2f ms", io.Framerate, 1000.f / std::max(1.f, io.Framerate));
    ImGui::Text("tile %d, %d   map %d", (int)(g.p.pos.x / TS), (int)(g.p.pos.y / TS), g.world.mapId);
    ImGui::Text("props %d   decor %d   nodes %d   lanterns %d", (int)g.world.props.size(), (int)g.world.decor.size(), (int)g.world.nodes.size(),
                (int)g.world.lanterns.size());
    ImGui::Text("animals %d   particles %d   structures %d", (int)g.animals.size(), (int)g.parts.size(), (int)g.structs.size());
    ImGui::Separator();
    ImGui::Checkbox("animal AI overlay (F3)", &g.debugAI);
    if (ImGui::Button("go to spawn")) {
        g.p.pos = g.freeSpot((int)(g.world.spawn.x / TS), (int)(g.world.spawn.y / TS));
        g.p.kb = V2();
        g.setCam(0, true);
    }
    ImGui::SameLine();
    if (ImGui::Button("screenshot")) {
        g_shotPath = "wildwood_devui.bmp";
        g_note = "saving wildwood_devui.bmp after this frame";
    }
    ImGui::SetNextItemWidth(110);
    ImGui::SliderFloat("panel scale", &io.FontGlobalScale, 0.8f, 2.0f, "%.1fx");
    ImGui::TextDisabled("F1 hides this panel. It is only in dev builds (-DWILDWOOD_DEV_UI=ON).");
}

}  // namespace

void init(Game& g) {
    if (g_inited || !g.ren || !g.win) return;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // no imgui.ini dropped next to the game
    io.LogFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 4.f;
    ImGui::GetStyle().Alpha = 0.96f;
    ImGui_ImplSDL2_InitForSDLRenderer(g.win, g.ren);
    ImGui_ImplSDLRenderer2_Init(g.ren);
    g_seed = g.worldSeed;
    g_inited = true;
}

void shutdown(Game& g) {
    if (!g_inited) return;
    if (g_seedTouched && g.worldSeed == g_seedSet) g.worldSeed = g_seedBefore;  // the save follows the game's own world, not the one being tuned
    if (g_tex) SDL_DestroyTexture(g_tex);
    g_tex = nullptr;
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    g_inited = false;
}

bool wantKeyboard() { return g_inited && g_open && ImGui::GetIO().WantTextInput; }

bool processEvent(Game& g, const SDL_Event& e) {
    if (!g_inited) return false;
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F1) {
        if (!e.key.repeat) {
            g_open = !g_open;
            SDL_ShowCursor(g_open ? SDL_ENABLE : SDL_DISABLE);  // the game draws its own cursor; the panel needs the real one
            if (g_open) g_seed = g.worldSeed;
        }
        return true;
    }
    if (!g_open) return false;
    ImGui_ImplSDL2_ProcessEvent(&e);
    const ImGuiIO& io = ImGui::GetIO();
    switch (e.type) {
        case SDL_MOUSEBUTTONDOWN:
            if (io.WantCaptureMouse) {
                if (e.button.button == SDL_BUTTON_LEFT) g_swallowLeft = true;
                return true;
            }
            return false;
        case SDL_MOUSEBUTTONUP:
            if (e.button.button == SDL_BUTTON_LEFT && g_swallowLeft) {
                g_swallowLeft = false;
                return true;
            }
            return false;
        case SDL_MOUSEMOTION: return false;  // the game keeps tracking the mouse (its own pointer), the panel reads it separately
        case SDL_MOUSEWHEEL: return io.WantCaptureMouse;
        case SDL_KEYDOWN: case SDL_KEYUP: case SDL_TEXTINPUT: case SDL_TEXTEDITING: return io.WantCaptureKeyboard || io.WantTextInput;
        default: return false;
    }
}

void draw(Game& g) {
    if (!g_inited || !g_open) return;
    if (g_freeze) g.dayT = g_freezeT;
    if (g_god) g.p.hp = g.p.maxhp;
    if (g_infSta) g.p.stamina = 100.f;
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(470, 560), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Wildwood dev  (F1)", &g_open)) {
        if (ImGui::BeginTabBar("devtabs")) {
            if (ImGui::BeginTabItem("World")) { tabWorld(g); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Map")) { tabMap(g); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Time")) { tabTime(g); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Spawn")) { tabSpawn(g); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Debug")) { tabDebug(g); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
        if (!g_note.empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("%s", g_note.c_str());
        }
    }
    ImGui::End();
    if (!g_open) SDL_ShowCursor(SDL_DISABLE);  // closed with the window's own x
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), g.ren);
    if (g_shotPath) {
        saveWindow(g, g_shotPath);
        g_shotPath = nullptr;
    }
}

}  // namespace devui
#endif  // GATHERING_DEV_UI
