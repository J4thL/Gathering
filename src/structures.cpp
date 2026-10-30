// structures.cpp - things the hunter places in the world: campfires and storage chests.
//
// A campfire is a crafting station (smelting, cooking), a warm spot (no cold drain, slow healing) and a light at night.
// A chest holds 20 more slots of items; it is opened with E and can only be picked up again when it is empty.
// Structures block movement (their tile is solid) and are saved with the game (struct / cslot lines).
#include <algorithm>
#include <sstream>
#include "game.hpp"

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
constexpr float FIRE_RANGE = 56.f;  // crafting range of a campfire

bool Game::canPlaceAt(int tx, int ty) const {
    if (protectedTile(tx, ty, world.mapId) || world.solidTile(tx, ty) || tileDeep(world.tile[ty][tx])) return false;
    const V2 c(tx * TS + 8.f, ty * TS + 8.f);
    if (dist(p.pos, c) < 10.f) return false;
    if (dist(c, world.keeper) < 26.f || dist(c, world.questGiver) < 26.f || dist(c, world.fire) < 26.f || dist(c, world.door) < 26.f) return false;
    for (auto& s : structs)
        if (s.tx == tx && s.ty == ty) return false;
    for (auto& n : world.nodes)  // not on top of a plant or ore vein
        if (fabsf(n.x - c.x) < 12.f && fabsf(n.y - c.y) < 12.f) return false;
    for (auto& a : animals)
        if (!a.dead && dist(a.pos, c) < 12.f) return false;
    return true;
}

// Place one campfire / chest on the free tile in front of the hunter (or beside them when the front is blocked).
bool Game::placeStructure(int kind) {
    if ((int)structs.size() >= MAX_STRUCTS) {
        msg("TOO MANY STRUCTURES - PICK ONE UP FIRST", C_RED);
        audio.play(S_ERROR);
        return false;
    }
    V2 f = p.dir == 0 ? V2(0, 1) : p.dir == 1 ? V2(0, -1) : (p.flip ? V2(-1, 0) : V2(1, 0));
    const V2 side(-f.y, f.x);
    const V2 tries[6] = {f * 18, f * 18 + side * 12, f * 18 - side * 12, side * 18, side * -18, f * -18};
    for (const V2& o : tries) {
        int tx = (int)floorf((p.pos.x + o.x) / TS), ty = (int)floorf((p.pos.y + o.y) / TS);
        if (!canPlaceAt(tx, ty)) continue;
        if (p.inv.remove(itemPlace(kind), 1) < 1) return false;
        Structure s;
        s.kind = kind;
        s.tx = tx;
        s.ty = ty;
        structs.push_back(s);
        world.solid[ty][tx] = true;
        audio.play(kind == PL_CAMPFIRE ? S_SMELT : S_CRAFT, 0.7f);
        harvestChips(s.pos() + V2(0, -4), HEX(kind == PL_CAMPFIRE ? 0xf0953a : 0x8a5a2c), 8, 1.f);
        msg(kind == PL_CAMPFIRE ? "CAMPFIRE LIT - CRAFT AND SMELT BESIDE IT" : "CHEST PLACED - E TO OPEN", C_GOLD);
        return true;
    }
    msg("NO ROOM HERE - FIND OPEN GROUND", C_RED);
    audio.play(S_ERROR);
    return false;
}

Structure* Game::structNear(float r, int kind) {
    Structure* best = nullptr;
    float bd = r;
    for (auto& s : structs) {
        if (kind >= 0 && s.kind != kind) continue;
        float d = dist(p.pos, s.pos());
        if (d < bd) bd = d, best = &s;
    }
    return best;
}

bool Game::nearFire() const {
    if (dist(p.pos, world.fire) < FIRE_RANGE) return true;  // the village campfire counts
    for (auto& s : structs)
        if (s.kind == PL_CAMPFIRE && dist(p.pos, s.pos()) < FIRE_RANGE) return true;
    return false;
}

float Game::warmthAt(V2 pos) const {
    float w = 0.f;
    auto add = [&](V2 fire) { w = std::max(w, clampf(1.f - dist(pos, fire) / 90.f, 0.f, 1.f)); };
    for (auto& s : structs)
        if (s.kind == PL_CAMPFIRE) add(s.pos());
    if (world.biomeAtPos(pos.x, pos.y) == B_SNOW) add(world.fire);  // only matters in the snow; the village fire is far from it
    return w;
}

void Game::updateStructures(float dt) {
    if (structs.empty() || p.dead) return;
    // standing by your own fire slowly mends wounds
    if (warmthAt(p.pos) > 0.35f && p.sinceHurt > 2.5f && p.hp < p.maxhp) p.hp = std::min(p.maxhp, p.hp + 2.5f * dt);
    // a little smoke and sparks
    for (auto& s : structs)
        if (s.kind == PL_CAMPFIRE && fabsf(s.pos().x - p.pos.x) < LW && fabsf(s.pos().y - p.pos.y) < LH && grng.chance(dt * 6.f))
            parts.push_back({s.pos().x + grng.range(-2, 2), s.pos().y - 6, 4, grng.range(-6, 6), grng.range(-3, 3), grng.range(24, 46), 0.7f, 0.7f,
                             grng.chance(0.4f) ? HEX(0xf2c94c) : HEX(0xd8d0c0), 1, false});
}

// E near a structure: chests open, campfires are picked up by holding E. Returns true while a structure is in reach (it takes the E key).
bool Game::structInteract(float dt) {
    Structure* s = structNear(24.f);
    if (!s || p.dead) {
        p.pickT = 0;
        return false;
    }
    if (s->kind == PL_CHEST) {
        p.pickT = 0;
        if (in.ePressed) {
            chestOpen = (int)(s - structs.data());
            chestSel = 0;
            state = ST_CHEST;
            audio.play(S_CLICK);
        }
        return true;
    }
    if (in.e && !p.charging) {
        p.pickT += dt;
        if (p.pickT >= 0.8f) {
            p.pickT = 0;
            if (!p.inv.canAdd(itemPlace(PL_CAMPFIRE), 1)) {
                msg("BAG FULL - CANNOT PICK UP THE CAMPFIRE", C_RED);
                audio.play(S_ERROR);
            } else {
                p.inv.add(itemPlace(PL_CAMPFIRE), 1);
                world.solid[s->ty][s->tx] = false;
                harvestChips(s->pos() + V2(0, -4), HEX(0x8a8a8a), 6, 1.f);
                structs.erase(structs.begin() + (s - structs.data()));
                msg("CAMPFIRE PUT OUT AND PACKED", C_DIM);
                audio.play(S_PICKUP);
            }
        }
    } else
        p.pickT = 0;
    return true;
}

void Game::drawStructPrompt() {
    Structure* s = structNear(24.f);
    if (!s || p.dead || state != ST_PLAY) return;
    const V2 c = s->pos();
    if (s->kind == PL_CHEST) {
        int used = s->inv.usedSlots();
        drawPrompt(c.x, c.y - 18, "E", "OPEN CHEST " + S(used) + "/" + S(CHEST_SLOTS));
    } else if (p.pickT > 0) {
        bar((int)c.x - camX - 14, (int)c.y - camY - 24, 28, 4, p.pickT / 0.8f, C_ORANGE);
    } else {
        drawPrompt(c.x, c.y - 18, "E", "HOLD TO PICK UP");
    }
}

// ----------------------------------------------------------------- saving
void Game::structSave(std::ostream& f) const {
    for (size_t i = 0; i < structs.size(); i++) {
        const Structure& s = structs[i];
        f << "struct " << s.kind << ' ' << s.tx << ' ' << s.ty << "\n";
    }
    for (size_t i = 0; i < structs.size(); i++)
        for (auto& sl : structs[i].inv.slots)
            if (!sl.empty()) f << "cslot " << i << ' ' << itemKey(sl.item) << ' ' << sl.n << "\n";
}

void Game::structLoad(const std::vector<std::string>& lines) {
    for (auto& ln : lines) {
        std::istringstream ss(ln);
        std::string tag;
        ss >> tag;
        if (tag == "struct") {
            int kind = -1, tx = 0, ty = 0;
            ss >> kind >> tx >> ty;
            if (!ss || kind < 0 || kind >= PL_COUNT || tx < 0 || ty < 0 || tx >= MW || ty >= MH || protectedTile(tx, ty, world.mapId) || (int)structs.size() >= MAX_STRUCTS) continue;
            bool dup = false;
            for (auto& o : structs) dup = dup || (o.tx == tx && o.ty == ty);
            if (dup) continue;
            Structure s;
            s.kind = kind;
            s.tx = tx;
            s.ty = ty;
            structs.push_back(s);
            world.solid[ty][tx] = true;
        }
    }
    for (auto& ln : lines) {
        std::istringstream ss(ln);
        std::string tag, key;
        int idx = -1, n = 0;
        ss >> tag >> idx >> key >> n;
        if (tag != "cslot" || !ss || idx < 0 || idx >= (int)structs.size() || structs[idx].kind != PL_CHEST) continue;
        int id = itemFromKey(key);
        if (id < 0 || n <= 0) continue;
        structs[idx].inv.add(id, std::min(n, 99999));
    }
}

// ----------------------------------------------------------------- chest screen
namespace {
constexpr int CH_COLS = 5, PK_COLS = 8, CELL = 21;
// move a whole stack from one inventory slot into another inventory; returns how many moved
int moveStack(Slot& src, Inventory& dst) {
    if (src.empty()) return 0;
    int got = dst.add(src.item, src.n);
    src.n -= got;
    if (src.n <= 0) src = Slot();
    return got;
}
}  // namespace

// fromPack: 1 = from the backpack into the chest, 0 = from the chest into the backpack
bool Game::chestMove(int fromPack, int slot) {
    if (chestOpen < 0 || chestOpen >= (int)structs.size()) return false;
    Inventory& chest = structs[chestOpen].inv;
    Inventory& pack = p.inv;
    Inventory& from = fromPack ? pack : chest;
    Inventory& to = fromPack ? chest : pack;
    if (slot < 0 || slot >= from.capacity() || from.slots[slot].empty()) return false;
    const int want = from.slots[slot].n;
    int moved = moveStack(from.slots[slot], to);
    if (moved < want) {
        msg(fromPack ? "CHEST FULL" : "BAG FULL", C_RED);
        audio.play(S_ERROR);
    }
    if (moved > 0) audio.play(S_CLICK, 0.5f);
    return moved > 0;
}

void Game::updateChest(float dt) {
    updateFx(dt);  // time stands still while the chest is open
    if (in.escPressed || in.ePressed || chestOpen < 0 || chestOpen >= (int)structs.size()) {
        state = ST_PLAY;
        chestOpen = -1;
        save();
        audio.play(S_CLICK);
    }
}

void Game::drawChest() {
    if (chestOpen < 0 || chestOpen >= (int)structs.size()) return;
    Structure& ch = structs[chestOpen];
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 150));
    const int X = 24, Y = 12, W = 352, H = 200;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, UI_BG);
    text(X + 8, Y + 7, "STORAGE CHEST", C_GOLD);
    const int cap = p.bagCap();
    textC(X + W / 2 + 40, Y + 7, "CLICK AN ITEM TO MOVE ITS WHOLE STACK", C_DIM, 1, false);
    const int cy = Y + 34;
    const int cgx = X + 8, cgy = cy + 12;                       // chest grid
    const int pgx = X + W - 8 - PK_COLS * CELL, pgy = cy + 12;  // pack grid
    text(cgx, cy + 2, "CHEST " + S(ch.inv.usedSlots()) + "/" + S(CHEST_SLOTS), C_TEXT);
    text(pgx, cy + 2, "PACK " + S(p.bagUsed()) + "/" + S(cap), p.bagUsed() >= cap ? C_RED : C_TEXT);
    auto cell = [&](Inventory& inv, int i, int gx, int gy, int cols, bool toPack) {
        int x = gx + (i % cols) * CELL, y = gy + (i / cols) * CELL;
        const Slot& sl = inv.slots[i];
        const bool hov = inRect(x, y, CELL - 1, CELL - 1);
        panel(x, y, CELL - 1, CELL - 1, hov ? HEX(0x4a3624) : HEX(0x33241a));
        if (!sl.empty()) {
            icon(itemIcon(sl.item), x + (CELL - 1) / 2, y + (CELL - 1) / 2 - 1, 14);
            if (sl.n > 1) {
                std::string n = S(sl.n);
                text(x + CELL - 2 - textW(n), y + CELL - 8, n, C_TEXT);
            }
        }
        if (hov && in.uiClick && !sl.empty()) {
            in.uiClick = false;
            in.lPressed = false;
            chestMove(toPack ? 0 : 1, i);
        }
        return hov;
    };
    int hoverItem = -1;
    for (int i = 0; i < ch.inv.capacity(); i++)
        if (cell(ch.inv, i, cgx, cgy, CH_COLS, true) && !ch.inv.slots[i].empty()) hoverItem = ch.inv.slots[i].item;
    for (int i = 0; i < cap; i++)
        if (cell(p.inv, i, pgx, pgy, PK_COLS, false) && !p.inv.slots[i].empty()) hoverItem = p.inv.slots[i].item;
    // buttons in the middle
    const int bx = cgx + CH_COLS * CELL + 6, bw = pgx - bx - 6;
    int by = cgy;
    if (button(bx, by, bw, 14, "STACK", true, HEX(0x4a3a5e))) {  // quick stack: everything the chest already holds goes in
        int moved = 0;
        for (auto& sl : p.inv.slots)
            if (!sl.empty() && ch.inv.count(sl.item) > 0) moved += moveStack(sl, ch.inv);
        msg(moved > 0 ? "STACKED " + S(moved) + " ITEMS" : "NOTHING TO STACK", C_DIM);
    }
    by += 17;
    if (button(bx, by, bw, 14, "TAKE ALL", ch.inv.usedSlots() > 0, HEX(0x5d7a2e))) {
        int left = 0;
        for (auto& sl : ch.inv.slots) {
            moveStack(sl, p.inv);
            if (!sl.empty()) left += sl.n;
        }
        if (left > 0) msg("BAG FULL - SOME ITEMS STAYED IN THE CHEST", C_RED);
    }
    by += 17;
    if (button(bx, by, bw, 14, "SORT", true, HEX(0x4a3a5e))) {
        ch.inv.compact();
        p.inv.compact();
    }
    by += 17;
    const bool empty = ch.inv.usedSlots() == 0;
    if (button(bx, by, bw, 14, "PICK UP", empty, HEX(0x7a3a2a))) {
        if (!p.inv.canAdd(itemPlace(PL_CHEST), 1)) {
            msg("BAG FULL - CANNOT PICK UP THE CHEST", C_RED);
            audio.play(S_ERROR);
        } else {
            p.inv.add(itemPlace(PL_CHEST), 1);
            world.solid[ch.ty][ch.tx] = false;
            structs.erase(structs.begin() + chestOpen);
            chestOpen = -1;
            state = ST_PLAY;
            msg("CHEST PACKED UP", C_DIM);
            audio.play(S_PICKUP);
            return;
        }
    }
    by += 17;
    if (!empty) text(bx, by + 1, "EMPTY IT TO", C_DIM, 1, false), text(bx, by + 9, "PICK IT UP", C_DIM, 1, false);
    // hovered item name
    if (hoverItem >= 0) {
        const ItemDef& D = item(hoverItem);
        textC(X + W / 2, Y + H - 24, D.name + "  -  " + D.blurb, C_TEXT, 1, false);
    }
    textC(X + W / 2, Y + H - 12, "E / ESC CLOSE", C_DIM, 1, false);
}
