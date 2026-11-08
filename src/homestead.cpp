// homestead.cpp - the Homestead's rules: the portal trip, building and demolishing, farming, domestic animals, interaction and saving.
// The map itself is homestead_gen.hpp; the menus are homestead_ui.cpp; drawing is homestead_render.cpp. State lives in Game::hs and is
// saved as hs / hsp / hsi / hsa lines, so the Homestead is never regenerated or reset: only terrain and props come from the world seed.
#include <algorithm>
#include <cmath>
#include <sstream>
#include "game.hpp"

namespace {
constexpr int PLAZA_X0 = HS_PLAZA_X - 7, PLAZA_X1 = HS_PLAZA_X + 11, PLAZA_Y0 = HS_PLAZA_Y - 8, PLAZA_Y1 = HS_PLAZA_Y + 8;  // kept clear: portal + market
inline bool inPlaza(int x, int y) { return x >= PLAZA_X0 && x <= PLAZA_X1 && y >= PLAZA_Y0 && y <= PLAZA_Y1; }
inline bool isRing(const HPiece& pc, int x, int y) { return x == pc.tx || y == pc.ty || x == pc.tx + pc.w() - 1 || y == pc.ty + pc.h() - 1; }
inline bool isGap(const HPiece& pc, int x, int y) { return y == pc.ty + pc.h() - 1 && x == pc.tx + (pc.w() - 1) / 2; }  // the pen's door: south middle
inline bool rectNear(const HPiece& pc, V2 p, float reach) {
    const float x0 = pc.tx * (float)TS, y0 = pc.ty * (float)TS, x1 = (pc.tx + pc.w()) * (float)TS, y1 = (pc.ty + pc.h()) * (float)TS;
    const float dx = std::max(std::max(x0 - p.x, 0.f), p.x - x1), dy = std::max(std::max(y0 - p.y, 0.f), p.y - y1);
    return dx * dx + dy * dy < reach * reach;
}
}  // namespace

// ================================================================= pieces: index, solids, world

void Game::hsReindex() {
    for (int l = 0; l < 3; l++) hsGrid[l].assign((size_t)MW * MH, 0);
    for (auto& pc : hs.pieces)
        for (int y = pc.ty; y < pc.ty + pc.h(); y++)
            for (int x = pc.tx; x < pc.tx + pc.w(); x++)
                if (x >= 0 && y >= 0 && x < MW && y < MH) hsGrid[HPDEF[pc.kind].layer][(size_t)y * MW + x] = pc.id;
}

void Game::hsReset() {
    hs = Homestead();
    bm = BuildMode();
    farmPieceId = animSelId = -1;
    hsChestPiece = -1;
    portalFx = 0;
    hsReindex();
}

HPiece* Game::hsPiece(int id) {
    if (id <= 0) return nullptr;
    for (auto& pc : hs.pieces)
        if (pc.id == id) return &pc;
    return nullptr;
}

HPiece* Game::hsAt(int tx, int ty, int layer) {
    if (tx < 0 || ty < 0 || tx >= MW || ty >= MH || hsGrid[layer].empty()) return nullptr;
    return hsPiece(hsGrid[layer][(size_t)ty * MW + tx]);
}

// solid tiles of a piece (doors and gates are solid only while closed; a pen is a fence ring with a gap)
void Game::hsSetSolid(const HPiece& pc, bool on) {
    const HPieceDef& D = HPDEF[pc.kind];
    if (D.solid == HS_NONE) return;
    if ((pc.kind == HP_DOOR || pc.kind == HP_GATE) && pc.open) on = false;
    for (int y = pc.ty; y < pc.ty + pc.h(); y++)
        for (int x = pc.tx; x < pc.tx + pc.w(); x++) {
            if (x < 0 || y < 0 || x >= MW || y >= MH) continue;
            if (D.solid == HS_RING && (!isRing(pc, x, y) || isGap(pc, x, y))) continue;
            world.solid[y][x] = on;
        }
}

void Game::hsApplyWorld() {
    for (auto& pc : hs.pieces) {
        if (pc.kind == HP_BRIDGE && pc.tx >= 0 && pc.ty >= 0 && pc.tx < MW && pc.ty < MH) world.tile[pc.ty][pc.tx] = T_DIRT;
        hsSetSolid(pc, true);
    }
}

// ================================================================= building

bool Game::hsCost(int kind, std::string* lack) const {
    const HPieceDef& D = HPDEF[kind];
    for (int i = 0; i < 3; i++)
        if (D.mat[i] >= 0 && D.n[i] > 0 && p.mat(D.mat[i]) < D.n[i]) {
            if (lack) *lack = "NEED " + S(D.n[i]) + " " + MAT_NAME[D.mat[i]];
            return false;
        }
    return true;
}

bool Game::hsCanBuild(int kind, int tx, int ty, int rot, std::string* why) {
    auto no = [&](const char* s) {
        if (why) *why = s;
        return false;
    };
    if (kind < 0 || kind >= HP_COUNT || !inHomestead()) return no("ONLY AT YOUR HOMESTEAD");
    const HPieceDef& D = HPDEF[kind];
    const int w = (rot & 1) ? D.h : D.w, h = (rot & 1) ? D.w : D.h;
    if ((int)hs.pieces.size() >= H_MAX_PIECES) return no("TOO MANY BUILDINGS");
    const bool needsHoe = kind == HP_PLOT;
    for (int y = ty; y < ty + h; y++)
        for (int x = tx; x < tx + w; x++) {
            if (x < 0 || y < 0 || x >= MW || y >= MH || protectedTile(x, y, B_HOMESTEAD)) return no("TOO CLOSE TO THE EDGE");
            if (inPlaza(x, y)) return no("KEEP THE PLAZA CLEAR");
            const uint8_t t = world.tile[y][x];
            const HPiece* gp = hsAt(x, y, HL_GROUND);
            const HPiece* op = hsAt(x, y, HL_OBJECT);
            if (kind == HP_BRIDGE) {
                if (t != T_WATER) return no("BRIDGES GO OVER WATER");
                if (gp) return no("ALREADY BUILT HERE");
                continue;
            }
            if (t != T_GRASS && t != T_DARK && t != T_DIRT) return no(tileDeep(t) ? "CANNOT BUILD ON WATER" : "CANNOT BUILD HERE");
            if (D.layer == HL_ROOF) {
                if (hsAt(x, y, HL_ROOF)) return no("ALREADY ROOFED");
                continue;
            }
            if (D.layer == HL_GROUND) {
                if (gp) return no("ALREADY BUILT HERE");
                if (op && op->kind != HP_GREENHOUSE) return no("SOMETHING IS IN THE WAY");
                if (needsHoe && !(t == T_GRASS || t == T_DARK || t == T_DIRT)) return no("SOIL TOO HARD");
                if (world.solid[y][x] && !op) return no("SOMETHING IS IN THE WAY");
                continue;
            }
            // object layer
            if (op) return no("SOMETHING IS IN THE WAY");
            if (gp && gp->kind == HP_PLOT && kind != HP_GREENHOUSE) return no("A FARM PLOT IS IN THE WAY");
            if (world.solid[y][x]) return no("SOMETHING IS IN THE WAY");
            for (auto& n : world.nodes)
                if ((int)floorf(n.x / TS) == x && (int)floorf(n.y / TS) == y) return no("A PLANT IS IN THE WAY");
            if (D.solid != HS_NONE) {  // never trap the hunter or an animal inside a new wall
                const float cx = x * (float)TS, cy = y * (float)TS;
                if (p.pos.x > cx - 5 && p.pos.x < cx + TS + 5 && p.pos.y > cy - 4 && p.pos.y < cy + TS + 5) return no("YOU ARE IN THE WAY");
                for (auto& a : hs.animals)
                    if (a.px > cx - 2 && a.px < cx + TS + 2 && a.py > cy - 2 && a.py < cy + TS + 2) return no("AN ANIMAL IS IN THE WAY");
            }
        }
    if (needsHoe && bestTool(TT_HOE, nullptr) <= 0) return no(p.inv.count(itemTool(TL_HOE)) > 0 ? "EQUIP YOUR HOE FIRST (PACK)" : "NEED A FARMER'S HOE");
    std::string lack;
    if (!hsCost(kind, &lack)) return no(lack.c_str());
    if (why) why->clear();
    return true;
}

// what demolishing gives back of material i of a piece: 60%, and at least one of the main material
static int hsRefund(const HPieceDef& D, int i) {
    if (D.mat[i] < 0 || D.n[i] <= 0) return 0;
    const int back = D.n[i] * 60 / 100;
    return i == 0 ? std::max(1, back) : back;
}

bool Game::hsPlace(int kind, int tx, int ty, int rot) {
    std::string why;
    if (!hsCanBuild(kind, tx, ty, rot, &why)) {
        msg(why, C_RED);
        audio.play(S_ERROR, 0.6f);
        return false;
    }
    const HPieceDef& D = HPDEF[kind];
    for (int i = 0; i < 3; i++)
        if (D.mat[i] >= 0 && D.n[i] > 0) p.inv.remove(itemMat(D.mat[i]), D.n[i]);  // only now: a refused build never costs anything
    HPiece pc;
    pc.id = hs.nextId++;
    pc.kind = kind;
    pc.tx = tx;
    pc.ty = ty;
    pc.rot = rot & 1;
    pc.inv = Inventory(D.storage, 1e9f);
    if (kind == HP_PLOT) pc.rich = world.tile[ty][tx] == T_DARK;
    if (kind == HP_BRIDGE) world.tile[ty][tx] = T_DIRT;
    hs.pieces.push_back(pc);
    hsSetSolid(hs.pieces.back(), true);
    hsReindex();
    // Building XP is paid for what the piece really costs: its materials minus what demolishing it gives back. A piece that comes back
    // in full (a plot, a 1-plank floor) would otherwise be a free XP loop of build / demolish / build.
    int net = 0;
    for (int i = 0; i < 3; i++) net += std::max(0, D.n[i]) - hsRefund(D, i);
    if (net > 0) grantXp(SK_BUILDING, 1 + net / 3);
    ftext(V2(tx * (float)TS + pc.w() * 8.f, ty * (float)TS), std::string(D.name) + " BUILT", C_GREEN);
    audio.play(S_CRAFT, 0.6f);
    return true;
}

bool Game::hsDemolish(int tx, int ty) {
    HPiece* pc = hsAt(tx, ty, HL_OBJECT);
    if (!pc) pc = hsAt(tx, ty, HL_ROOF);
    if (!pc) pc = hsAt(tx, ty, HL_GROUND);
    if (!pc) {
        msg("NOTHING TO DEMOLISH HERE", C_DIM);
        return false;
    }
    const HPieceDef& D = HPDEF[pc->kind];
    if (D.storage > 0 && pc->inv.usedSlots() > 0) {
        msg("EMPTY THE " + std::string(D.name) + " FIRST", C_RED);
        audio.play(S_ERROR, 0.6f);
        return false;
    }
    if (pc->kind == HP_BRIDGE && (int)floorf(p.pos.x / TS) == pc->tx && (int)floorf(p.pos.y / TS) == pc->ty) {
        msg("YOU ARE STANDING ON IT", C_RED);
        return false;
    }
    const int id = pc->id, kind = pc->kind;
    hsSetSolid(*pc, false);
    if (kind == HP_BRIDGE) world.tile[pc->ty][pc->tx] = T_WATER;
    for (auto& a : hs.animals)
        if (a.home == id) a.home = -1;  // the animals are not lost: they roam free
    int got = 0;
    for (int i = 0; i < 3; i++) {
        if (D.mat[i] < 0 || D.n[i] <= 0) continue;
        const int back = hsRefund(D, i);
        if (back <= 0) continue;
        const int in = p.inv.addIgnoringWeight(itemMat(D.mat[i]), back);
        got += in;
        if (in < back) {  // a full pack: the rest lies on the ground
            GroundItem g{};
            g.pos = p.pos;
            g.item = itemMat(D.mat[i]);
            g.n = back - in;
            g.ammo = -1;
            items.push_back(g);
        }
    }
    hs.pieces.erase(std::remove_if(hs.pieces.begin(), hs.pieces.end(), [&](const HPiece& q) { return q.id == id; }), hs.pieces.end());
    hsReindex();
    if (hsChestPiece == id) hsChestPiece = -1;
    ftext(p.pos + V2(0, -22), got > 0 ? "+" + S(got) + " MATERIALS" : "REMOVED", C_ORANGE);
    audio.play(S_BREAK, 0.6f);
    return true;
}

// ================================================================= the portal

void Game::hsGiveStarter() {
    if (hs.gaveStarter) return;
    hs.gaveStarter = true;
    auto give = [&](int item, int n) {
        if (p.inv.add(item, n) < n) p.inv.addIgnoringWeight(item, n);
    };
    // the starter tools go straight into the equipment slots (a hunter who already owns one, equipped or not, is not given a second)
    for (int k : {(int)TL_HOE, (int)TL_CAN, (int)TL_SICKLE})
        if (!ownsTool(k)) p.tool[TOOLDEF[k].type] = k;
    give(itemMat(M_SEED_WHEAT), 6);
    give(itemMat(M_SEED_CARROT), 6);
    give(itemMat(M_SEED_POTATO), 4);
    msg("WELCOME HOME! A HOE, WATERING CAN, SICKLE AND SEEDS WERE LEFT FOR YOU", C_GOLD);
}

bool Game::hsEnter() {
    if (world.mapId != B_HOME || interior >= 0 || delve >= 0) return false;
    hs.visited = true;
    hsGiveStarter();
    if (!travelTo(B_HOMESTEAD, HS_PORTAL_X, HS_PORTAL_Y + 2)) return false;
    travelFade = 0;  // the portal has its own transition
    portalFx = 1.4f;
    return true;
}

bool Game::hsLeave() {
    if (!inHomestead()) return false;
    if (!travelTo(B_HOME, COX + 33, COY + 33)) return false;
    travelFade = 0;
    portalFx = 1.4f;
    return true;
}

// ================================================================= farming

namespace {
bool inGreenhouse(const Homestead& hs, const HPiece& plot) {
    for (auto& g : hs.pieces)
        if (g.kind == HP_GREENHOUSE && g.covers(plot.tx, plot.ty)) return true;
    return false;
}
}  // namespace

void Game::hsTickCrops(float dt) {
    for (auto& pc : hs.pieces) {
        if (pc.kind != HP_PLOT || pc.crop < 0 || pc.crop >= CR_COUNT || pc.stage >= CROP_STAGES - 1) continue;
        const CropDef& C = CROPS[pc.crop];
        const bool gh = inGreenhouse(hs, pc);
        if (pc.water <= 0.f) continue;  // dry soil: the crop waits
        float grow = 1.f;
        if (gh) grow *= GREENHOUSE_GROWTH;
        if (pc.rich) grow *= 1.2f;
        pc.growT += dt * grow;
        pc.water = std::max(0.f, pc.water - dt * C.thirst * (gh ? GREENHOUSE_THIRST : 1.f) / PLOT_DRY_SEC);
        if (pc.growT >= C.stageSec) {
            pc.growT -= C.stageSec;
            pc.stage++;
            if (pc.stage >= CROP_STAGES - 1) pc.growT = 0;
        }
    }
}

void Game::hsRefillCan() {
    if (bestTool(TT_CAN, nullptr) <= 0) {
        msg("YOU NEED A WATERING CAN", C_RED);
        return;
    }
    hs.can = CAN_CAPACITY;
    msg("WATERING CAN FILLED", C_BLUE);
    audio.play(S_PICKUP, 0.5f);
}

bool Game::hsWater(HPiece& pc) {
    if (bestTool(TT_CAN, nullptr) <= 0) {
        msg("YOU NEED A WATERING CAN", C_RED);
        audio.play(S_ERROR, 0.6f);
        return false;
    }
    if (hs.can < 1.f) {
        msg("CAN IS EMPTY - REFILL AT A WELL OR THE WATER", C_RED);
        audio.play(S_ERROR, 0.6f);
        return false;
    }
    if (pc.water > 0.85f) {
        msg("THE SOIL IS ALREADY WET", C_DIM);
        return false;
    }
    hs.can -= 1.f;
    pc.water = 1.f;
    ftext(V2(pc.tx * (float)TS + 8.f, pc.ty * (float)TS), "WATERED", C_BLUE);
    audio.play(S_GATHER, 0.4f);
    return true;
}

bool Game::hsPlant(HPiece& pc, int crop) {
    if (pc.kind != HP_PLOT || pc.crop >= 0 || crop < 0 || crop >= CR_COUNT) return false;
    if (p.inv.remove(itemMat(cropSeed(crop)), 1) < 1) {
        msg("NO " + std::string(CROPS[crop].name) + " SEEDS", C_RED);
        return false;
    }
    pc.crop = crop;
    pc.stage = 0;
    pc.growT = 0;
    ftext(V2(pc.tx * (float)TS + 8.f, pc.ty * (float)TS), std::string(CROPS[crop].name) + " PLANTED", C_GREEN);
    audio.play(S_GATHER, 0.5f);
    grantXp(SK_FARMING, 1);
    return true;
}

namespace {
const char* TOOL_VERB[TT_COUNT] = {"AN AXE", "A PICKAXE", "A FARMER'S HOE", "A WATERING CAN", "A SICKLE"};
}

// gives items to the hunter; whatever does not fit lies at their feet (nothing is ever lost)
static void hsGive(Game& g, int mat, int n) {
    if (n <= 0) return;
    int in = g.p.inv.add(itemMat(mat), n);
    if (in < n) in += g.p.inv.addIgnoringWeight(itemMat(mat), n - in);
    if (in < n) {
        GroundItem it{};
        it.pos = g.p.pos;
        it.item = itemMat(mat);
        it.n = n - in;
        it.ammo = -1;
        g.items.push_back(it);
        g.msg("BAG FULL - SOME " + std::string(MAT_NAME[mat]) + " DROPPED", C_RED);
    }
}

bool Game::hsHarvest(HPiece& pc) {
    if (pc.kind != HP_PLOT || pc.crop < 0 || pc.stage < CROP_STAGES - 1) return false;
    const CropDef& C = CROPS[pc.crop];
    if (C.tool >= 0 && bestTool(C.tool, nullptr) <= 0) {
        msg(std::string(C.name) + " NEEDS " + TOOL_VERB[C.tool], C_RED);
        audio.play(S_ERROR, 0.6f);
        return false;
    }
    int n = grng.irange(C.yLo, C.yHi);
    if (grng.chance(skillBonus(SK_FARMING))) n++;
    if (pc.rich && grng.chance(0.25f)) n++;
    const int seeds = grng.irange(C.seedsLo, C.seedsHi);
    hsGive(*this, cropProduce(pc.crop), n);
    hsGive(*this, cropSeed(pc.crop), seeds);
    ftext(V2(pc.tx * (float)TS + 8.f, pc.ty * (float)TS - 2), "+" + S(n) + " " + C.name, C_GOLD);
    grantXp(SK_FARMING, C.xp);
    audio.play(S_PICKUP, 0.6f);
    if (C.regrow) {
        pc.stage = CROP_STAGES - 2;  // tomatoes and strawberries bear again
        pc.growT = 0;
    } else {
        pc.crop = -1;
        pc.stage = 0;
        pc.growT = 0;
    }
    return true;
}

void Game::hsFarmPlot(HPiece& pc) {
    if (pc.crop < 0) {
        farmPieceId = pc.id;
        state = ST_FARM;
        audio.play(S_CLICK);
    } else if (pc.stage >= CROP_STAGES - 1)
        hsHarvest(pc);
    else
        hsWater(pc);
}

// ================================================================= animals

int Game::hsHousingCount(int pieceId) const {
    int n = 0;
    for (auto& a : hs.animals)
        if (a.home == pieceId) n++;
    return n;
}

void Game::hsAddAnimal(int kind, V2 at, bool baby, int home) {
    if (kind < 0 || kind >= DA_COUNT) return;
    HAnimal a;
    a.id = hs.nextId++;
    a.kind = kind;
    a.px = a.tx = at.x;
    a.py = a.ty = at.y;
    a.age = baby ? 0.f : DOMS[kind].growSec + 1.f;
    a.home = home;
    a.breedCd = BREED_CD * 0.5f;
    a.face8 = frng.irange(0, F_COUNT - 1);
    a.stT = frng.range(0.5f, 3.f);
    hs.animals.push_back(a);
}

bool Game::hsAssign(HAnimal& a, int pieceId) {
    if (pieceId <= 0) {
        a.home = -1;
        return true;
    }
    HPiece* pc = hsPiece(pieceId);
    if (!pc || HPDEF[pc->kind].housing <= 0) return false;
    if (pc->kind == HP_COOP && !DOMS[a.kind].small) {
        msg("THE COOP ONLY HOLDS BIRDS AND RABBITS", C_RED);
        return false;
    }
    if (a.home != pieceId && hsHousingCount(pieceId) >= HPDEF[pc->kind].housing) {
        msg(std::string(HPDEF[pc->kind].name) + " IS FULL", C_RED);
        return false;
    }
    a.home = pieceId;
    return true;
}

// the area an animal wanders in (pixels): the pen's inside, the ground around a coop / barn, or the meadow by the plaza
bool Game::hsYard(const HAnimal& a, float& x0, float& y0, float& x1, float& y1) const {
    const HPiece* pc = nullptr;
    if (a.home > 0)
        for (auto& q : hs.pieces)
            if (q.id == a.home) pc = &q;
    if (pc) {
        const float T = (float)TS;
        if (pc->kind == HP_PEN) {
            x0 = (pc->tx + 1) * T + 3; y0 = (pc->ty + 1) * T + 4;
            x1 = (pc->tx + pc->w() - 1) * T - 3; y1 = (pc->ty + pc->h() - 1) * T - 2;
        } else {
            const int m = pc->kind == HP_BARN ? 3 : 2;
            x0 = (pc->tx - m) * T; y0 = (pc->ty - m) * T;
            x1 = (pc->tx + pc->w() + m) * T; y1 = (pc->ty + pc->h() + m) * T;
        }
        return true;
    }
    x0 = (HS_PLAZA_X - 13) * (float)TS; y0 = (HS_PLAZA_Y + 4) * (float)TS;
    x1 = (HS_PLAZA_X + 7) * (float)TS;  y1 = (HS_PLAZA_Y + 12) * (float)TS;
    return false;
}

int Game::hsFeedItem(const HAnimal& a) const {
    const DomDef& D = DOMS[a.kind];
    if (p.mat(D.feedA) > 0) return D.feedA;
    if (p.mat(D.feedB) > 0) return D.feedB;
    for (int m : {M_HAY, M_WHEAT, M_CARROT, M_CABBAGE, M_CORN, M_POTATO, M_PUMPKIN, M_BERRY})
        if (p.mat(m) > 0) return m;
    return -1;
}

static void feedWith(HAnimal& a, int mat) {
    const DomDef& D = DOMS[a.kind];
    const bool fav = mat == D.feedA || mat == D.feedB;
    a.full = std::min(FEED_FULL, a.full + (fav ? 45.f : 28.f));
    a.happy = std::min(100.f, a.happy + (fav ? 9.f : 3.f));
    a.state = 2;
    a.stT = 1.8f;
}

bool Game::hsFeed(HAnimal& a) {
    if (a.full > 92.f) {
        msg(std::string(DOMS[a.kind].name) + " IS FULL", C_DIM);
        return false;
    }
    const int m = hsFeedItem(a);
    if (m < 0) {
        msg("YOU HAVE NOTHING TO FEED IT - TRY HAY OR CROPS", C_RED);
        audio.play(S_ERROR, 0.6f);
        return false;
    }
    p.inv.remove(itemMat(m), 1);
    feedWith(a, m);
    ftext(V2(a.px, a.py - 16), "YUM!", C_GREEN);
    audio.play(S_GATHER, 0.4f);
    return true;
}

int Game::hsCollect(HAnimal& a) {
    const DomDef& D = DOMS[a.kind];
    if (a.stock <= 0 || D.product < 0) return 0;
    int n = a.stock;
    a.stock = 0;
    if (grng.chance(skillBonus(SK_FARMING))) n++;  // the FARMING bonus is "extra crops AND produce": crops get it in hsPick, produce here
    hsGive(*this, D.product, n);
    grantXp(SK_FARMING, 2 * n);
    ftext(V2(a.px, a.py - 16), "+" + S(n) + " " + D.prodName, C_GOLD);
    audio.play(S_PICKUP, 0.5f);
    return n;
}

void Game::hsTickAnimals(float dt) {
    const float T = (float)TS;
    struct Birth { int kind, home; V2 at; };
    std::vector<Birth> births;
    hs.breedT += dt;
    const bool breedTick = hs.breedT >= 5.f;
    if (breedTick) hs.breedT = 0;
    for (auto& a : hs.animals) {
        const DomDef& D = DOMS[a.kind];
        const bool adult = a.adult(D);
        // needs
        a.full = std::max(0.f, a.full - FEED_DECAY * dt);
        const HPiece* home = nullptr;
        if (a.home > 0)
            for (auto& q : hs.pieces)
                if (q.id == a.home) home = &q;
        if (home && home->kind == HP_BARN && a.full < 45.f)  // a barn feeds its animals from its own storage
            for (int m : {D.feedA, D.feedB, (int)M_HAY, (int)M_WHEAT, (int)M_CARROT, (int)M_CABBAGE, (int)M_CORN, (int)M_POTATO, (int)M_PUMPKIN, (int)M_BERRY})
                if (home->inv.count(itemMat(m)) > 0) {
                    const_cast<HPiece*>(home)->inv.remove(itemMat(m), 1);
                    feedWith(a, m);
                    break;
                }
        if (a.full > 60.f) a.happy = std::min(100.f, a.happy + 0.6f * dt);
        else if (a.full < 25.f) a.happy = std::max(0.f, a.happy - (a.full <= 0.f ? 2.f : 0.8f) * dt);
        if (a.full <= 0.f) a.health = std::max(10.f, a.health - 1.5f * dt);  // hungry animals get weak but never die
        else if (a.full > 40.f && a.health < 100.f) a.health = std::min(100.f, a.health + 0.8f * dt);
        a.age += dt * (a.full > 20.f ? 1.f : 0.3f);
        a.breedCd = std::max(0.f, a.breedCd - dt);
        // production
        if (adult && D.product >= 0 && a.full >= 30.f && a.happy >= 40.f && a.health >= 30.f) {
            a.prodT += dt * (0.5f + 0.5f * a.happy / 100.f);
            if (a.prodT >= D.prodSec) {
                a.prodT = 0;
                if (a.stock < D.prodMax) a.stock++;
            }
        }
        // movement
        a.animT += dt;
        a.stT -= dt;
        float x0, y0, x1, y1;
        hsYard(a, x0, y0, x1, y1);
        const float spd = D.speed * (adult ? 1.f : 0.8f);
        if (a.state == 1) {
            V2 d(a.tx - a.px, a.ty - a.py);
            const float len = d.len();
            if (len < 2.f || a.stT <= 0) {
                a.state = 0;
                a.stT = frng.range(1.5f, 4.5f);
            } else {
                const V2 step = d * (spd * dt / len);
                const float r = D.small ? 2.5f : 4.f;
                bool moved = false;
                if (!world.blocked(a.px + step.x, a.py, r)) { a.px += step.x; moved = true; }
                if (!world.blocked(a.px, a.py + step.y, r)) { a.py += step.y; moved = true; }
                if (!moved) {
                    a.state = 0;
                    a.stT = frng.range(0.5f, 1.5f);
                }
                a.face8 = facingFromDir(d.x, d.y, a.face8);
            }
        } else if (a.state == 2) {
            if (a.stT <= 0) { a.state = 0; a.stT = frng.range(1.f, 3.f); }
        } else if (a.stT <= 0) {
            if (a.full < 70.f && frng.chance(0.3f) && a.full > 0.f) {
                a.state = 2;  // grazing
                a.stT = 2.f;
            } else {
                for (int tries = 0; tries < 8; tries++) {
                    const float nx = frng.range(x0, x1), ny = frng.range(y0, y1);
                    if (!world.blocked(nx, ny, 3.f) && !tileDeep(world.tile[std::min(MH - 1, std::max(0, (int)(ny / T)))][std::min(MW - 1, std::max(0, (int)(nx / T)))])) {
                        a.tx = nx; a.ty = ny;
                        a.state = 1;
                        a.stT = 12.f;
                        break;
                    }
                }
                if (a.state != 1) a.stT = 1.5f;
            }
        }
        // strayed outside its yard (e.g. just assigned a home): walk back towards it
        if (a.state == 0 && (a.px < x0 - 8 || a.px > x1 + 8 || a.py < y0 - 8 || a.py > y1 + 8) && a.home > 0) {
            a.tx = (x0 + x1) / 2; a.ty = (y0 + y1) / 2;
            a.state = 1; a.stT = 20.f;
        }
        // breeding
        if (breedTick && adult && a.breedCd <= 0 && a.full >= 60.f && a.happy >= 60.f && a.health >= 60.f &&
            (int)hs.animals.size() + (int)births.size() < H_MAX_ANIMALS) {
            for (auto& b : hs.animals) {
                if (&b == &a || b.kind != a.kind || !b.adult(DOMS[b.kind]) || b.breedCd > 0 || b.home != a.home) continue;
                if (b.full < 60.f || b.happy < 60.f || b.health < 60.f) continue;
                if (hypotf(b.px - a.px, b.py - a.py) > 60.f) continue;
                if (a.home > 0) {
                    const HPiece* hp = nullptr;
                    for (auto& q : hs.pieces) if (q.id == a.home) hp = &q;
                    if (!hp || hsHousingCount(a.home) + (int)std::count_if(births.begin(), births.end(), [&](const Birth& x) { return x.home == a.home; }) >= HPDEF[hp->kind].housing) break;
                }
                a.breedCd = b.breedCd = BREED_CD;
                births.push_back({a.kind, a.home, V2((a.px + b.px) / 2, (a.py + b.py) / 2)});
                break;
            }
        }
    }
    for (auto& b : births) {
        hsAddAnimal(b.kind, b.at, true, b.home);
        msg(std::string("A ") + DOMS[b.kind].babyName + " WAS BORN!", C_GOLD);
    }
}

// ================================================================= per frame

void Game::updateHomestead(float dt) {
    if (portalFx > 0) portalFx = std::max(0.f, portalFx - dt);
    if (hsMsgT > 0) hsMsgT -= dt;
    if (!hs.visited && hs.pieces.empty() && hs.animals.empty()) return;
    hs.clock += dt;
    hsTickCrops(dt);
    hsTickAnimals(dt);
}

// ================================================================= interaction

bool Game::hsFindTarget(HTarget2& t) {
    t = HTarget2();
    if (interior >= 0 || delve >= 0 || p.dead) return false;
    float best = 1e9f;
    auto offer = [&](int type, int id, V2 pos, float d, const std::string& label, const std::string& key = "E") {
        if (d < best) { best = d; t.type = type; t.id = id; t.pos = pos; t.label = label; t.key = key; }
    };
    if (world.hportal.x > -9999.f) {
        const float d = dist(p.pos, world.hportal);
        if (d < 34.f) offer(8, 0, world.hportal + V2(0, -30), d - 20.f, world.mapId == B_HOME ? "ENTER HOMESTEAD PORTAL" : "RETURN TO TOWN");
    }
    if (!inHomestead()) return t.type != 0;
    if (world.hmarket.x > -9999.f) {
        const float d = dist(p.pos, world.hmarket);
        if (d < 34.f) offer(7, 0, world.hmarket + V2(0, -34), d - 14.f, "HOMESTEAD MARKET");
    }
    for (auto& pc : hs.pieces) {
        const HPieceDef& D = HPDEF[pc.kind];
        const V2 c((pc.tx + pc.w() / 2.f) * TS, (pc.ty + pc.h() / 2.f) * TS);
        if (pc.kind == HP_PLOT) {
            const float d = dist(p.pos, c);
            if (d < 26.f) {
                std::string lb = pc.crop < 0 ? "PLANT SEEDS" : pc.stage >= CROP_STAGES - 1 ? std::string("HARVEST ") + CROPS[pc.crop].name : (pc.water > 0.85f ? std::string(CROPS[pc.crop].name) + " - WET" : std::string("WATER ") + CROPS[pc.crop].name);
                offer(1, pc.id, c + V2(0, -14), d, lb);
            }
        } else if (pc.kind == HP_DOOR || pc.kind == HP_GATE) {
            const float d = dist(p.pos, c);
            if (d < 26.f) offer(4, pc.id, c + V2(0, -18), d + 2.f, std::string(pc.open ? "CLOSE " : "OPEN ") + D.name);
        } else if (D.storage > 0 && rectNear(pc, p.pos, 16.f)) {
            offer(3, pc.id, c + V2(0, -pc.h() * 8.f - 14), 12.f, std::string("OPEN ") + D.name);
        } else if (D.housing > 0 && rectNear(pc, p.pos, 16.f)) {
            offer(9, pc.id, c + V2(0, -pc.h() * 8.f - 14), 14.f, std::string("MANAGE ") + D.name);
        } else if (pc.kind == HP_WELL && rectNear(pc, p.pos, 16.f)) {
            offer(5, pc.id, c + V2(0, -22), 12.f, hs.can >= CAN_CAPACITY ? "WELL (CAN IS FULL)" : "REFILL WATERING CAN");
        }
    }
    for (auto& a : hs.animals) {
        const float d = dist(p.pos, V2(a.px, a.py));
        if (d < 24.f) offer(2, a.id, V2(a.px, a.py - 22), d - 4.f, std::string(a.adult(DOMS[a.kind]) ? DOMS[a.kind].name : DOMS[a.kind].babyName) + (a.stock > 0 ? " - COLLECT" : " - CARE"));
    }
    if (t.type == 0 && bestTool(TT_CAN, nullptr) > 0 && hs.can < CAN_CAPACITY) {  // the water's edge also fills the can
        const int cx = (int)floorf(p.pos.x / TS), cy = (int)floorf(p.pos.y / TS);
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (world.water(cx + dx, cy + dy)) offer(6, 0, p.pos + V2(0, -24), 20.f, "FILL WATERING CAN");
    }
    return t.type != 0;
}

bool Game::hsOutranksHarvest() {
    HTarget2 t;
    return inHomestead() && hsFindTarget(t) && t.type != 6;  // (the water's edge fill must not take E from a tree)
}

bool Game::homesteadInteract() {
    HTarget2 t;
    if (!hsFindTarget(t)) return false;
    switch (t.type) {
        case 8:
            if (world.mapId == B_HOME) hsEnter();
            else hsLeave();
            return true;
        case 7:
            state = ST_MARKET;
            marketScroll = 0;
            audio.play(S_CLICK);
            return true;
        case 1:
            if (HPiece* pc = hsPiece(t.id)) hsFarmPlot(*pc);
            return true;
        case 4:
            if (HPiece* pc = hsPiece(t.id)) {
                pc->open = !pc->open;
                hsSetSolid(*pc, true);
                if (!pc->open) {  // closing never traps the hunter: if they stand in the gap, push them clear
                    const float cx = pc->tx * (float)TS, cy = pc->ty * (float)TS;
                    if (p.pos.x > cx - 4 && p.pos.x < cx + TS + 4 && p.pos.y > cy - 3 && p.pos.y < cy + TS + 4) { pc->open = true; hsSetSolid(*pc, true); msg("SOMEONE IS IN THE WAY", C_DIM); return true; }
                }
                audio.play(S_CLICK, 0.6f);
            }
            return true;
        case 3:
            hsChestPiece = t.id;
            chestOpen = -1;
            state = ST_CHEST;
            audio.play(S_CLICK);
            return true;
        case 9:
            animSelId = -1;
            animScroll = 0;
            farmPieceId = t.id;  // the building the menu lists first
            state = ST_ANIMAL;
            audio.play(S_CLICK);
            return true;
        case 2:
            animSelId = t.id;
            animScroll = 0;
            farmPieceId = -1;
            state = ST_ANIMAL;
            audio.play(S_CLICK);
            return true;
        case 5:
        case 6:
            hsRefillCan();
            return true;
    }
    return false;
}

bool Game::homesteadPrompt() {
    HTarget2 t;
    if (!hsFindTarget(t)) return false;
    drawPrompt(t.pos.x, t.pos.y, t.key, t.label);
    return true;
}

// ================================================================= the open storage

Inventory* Game::openInv() {
    if (hsChestPiece > 0) {
        HPiece* pc = hsPiece(hsChestPiece);
        return pc ? &pc->inv : nullptr;
    }
    if (chestOpen >= 0 && chestOpen < (int)structs.size()) return &structs[chestOpen].inv;
    return nullptr;
}

// ================================================================= saving

std::string Game::hsSaveLines() {
    if (!hs.visited && hs.pieces.empty() && hs.animals.empty()) return "";
    std::ostringstream f;
    f << "hs " << hs.can << ' ' << (hs.visited ? 1 : 0) << ' ' << (hs.gaveStarter ? 1 : 0) << ' ' << hs.clock << ' ' << hs.nextId << "\n";
    for (auto& pc : hs.pieces) {
        f << "hsp " << pc.id << ' ' << pc.kind << ' ' << pc.tx << ' ' << pc.ty << ' ' << pc.rot << ' ' << (pc.open ? 1 : 0) << ' ' << pc.crop << ' ' << pc.stage
          << ' ' << pc.growT << ' ' << pc.water << ' ' << (pc.rich ? 1 : 0) << "\n";
        for (int i = 0; i < (int)pc.inv.slots.size(); i++)
            if (!pc.inv.slots[i].empty()) f << "hsi " << pc.id << ' ' << i << ' ' << itemKey(pc.inv.slots[i].item) << ' ' << pc.inv.slots[i].n << "\n";
    }
    for (auto& a : hs.animals)
        f << "hsa " << a.id << ' ' << a.kind << ' ' << a.px << ' ' << a.py << ' ' << a.age << ' ' << a.full << ' ' << a.happy << ' ' << a.health << ' ' << a.prodT
          << ' ' << a.stock << ' ' << a.breedCd << ' ' << a.home << "\n";
    return f.str();
}

void Game::hsLoadLine(const std::string& key, std::istringstream& ss) {
    if (key == "hs") {
        int vis = 0, st = 0;
        ss >> hs.can >> vis >> st >> hs.clock >> hs.nextId;
        hs.visited = vis != 0;
        hs.gaveStarter = st != 0;
        hs.can = std::max(0.f, std::min(CAN_CAPACITY, hs.can));
        hs.nextId = std::max(1, hs.nextId);
    } else if (key == "hsp") {
        HPiece pc;
        int open = 0, rich = 0;
        ss >> pc.id >> pc.kind >> pc.tx >> pc.ty >> pc.rot >> open >> pc.crop >> pc.stage >> pc.growT >> pc.water >> rich;
        if (!ss || pc.kind < 0 || pc.kind >= HP_COUNT || pc.id <= 0 || pc.tx < 0 || pc.ty < 0 || pc.tx >= MW || pc.ty >= MH) return;
        pc.rot &= 1;
        pc.open = open != 0;
        pc.rich = rich != 0;
        if (pc.crop < -1 || pc.crop >= CR_COUNT) pc.crop = -1;
        pc.stage = std::max(0, std::min(CROP_STAGES - 1, pc.stage));
        pc.inv = Inventory(HPDEF[pc.kind].storage, 1e9f);
        hs.nextId = std::max(hs.nextId, pc.id + 1);
        hs.pieces.push_back(pc);
    } else if (key == "hsi") {
        int id = 0, idx = 0, n = 0;
        std::string k;
        ss >> id >> idx >> k >> n;
        HPiece* pc = hsPiece(id);
        const int item = itemFromKey(k);
        if (ss && pc && item >= 0 && n > 0 && idx >= 0 && idx < (int)pc->inv.slots.size()) {
            pc->inv.slots[idx].item = item;
            pc->inv.slots[idx].n = std::min(n, ::item(item).stack);
        }
    } else if (key == "hsa") {
        HAnimal a;
        ss >> a.id >> a.kind >> a.px >> a.py >> a.age >> a.full >> a.happy >> a.health >> a.prodT >> a.stock >> a.breedCd >> a.home;
        if (!ss || a.kind < 0 || a.kind >= DA_COUNT || a.id <= 0) return;
        a.tx = a.px;
        a.ty = a.py;
        a.full = std::max(0.f, std::min(100.f, a.full));
        a.happy = std::max(0.f, std::min(100.f, a.happy));
        a.health = std::max(0.f, std::min(100.f, a.health));
        a.stock = std::max(0, std::min(DOMS[a.kind].prodMax, a.stock));
        hs.nextId = std::max(hs.nextId, a.id + 1);
        hs.animals.push_back(a);
    }
}
