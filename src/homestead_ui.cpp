// homestead_ui.cpp - the Homestead's menus and the build-mode input: build menu, seed menu, animal management, market stall.
#include <algorithm>
#include <cmath>
#include "game.hpp"

namespace {
constexpr int PX = 24, PY = 12, PW = 352, PH = 200;
void frame(Game& g, const std::string& title) {
    g.fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 150));
    g.panel(PX, PY, PW, PH, HEX(0x4a3524));
    g.fillRect(PX + 3, PY + 3, PW - 6, 14, UI_BG);
    g.text(PX + 8, PY + 7, title, C_GOLD);
}
struct ToolBuy { int tool; int price; };
const ToolBuy TOOL_BUY[] = {{TL_HOE, 30}, {TL_CAN, 25}, {TL_SICKLE, 30}};
constexpr int HAY_PRICE = 4;
}  // namespace

// ================================================================= build mode (in the world)

void Game::updateBuildMode() {
    in.lReleased = false;
    const bool click = in.lPressed, held = in.l;
    in.lPressed = in.l = false;  // build mode eats the mouse: no weapon throws
    in.uiClick = false;
    if (in.buildToggle) {
        bm.on = false;
        state = ST_BUILD;
        return;
    }
    if (in.rotPressed) {
        bm.rot ^= 1;
        audio.play(S_CLICK, 0.4f);
    }
    if (in.delPressed) {
        bm.demolish = !bm.demolish;
        audio.play(S_CLICK, 0.4f);
    }
    const float wx = in.mx + camX, wy = in.my + camY;
    const HPieceDef& D = HPDEF[bm.kind];
    const int w = (bm.rot & 1) ? D.h : D.w, h = (bm.rot & 1) ? D.w : D.h;
    int tx, ty;
    if (bm.demolish) {
        tx = (int)floorf(wx / TS);
        ty = (int)floorf(wy / TS);
    } else {
        tx = (int)floorf((wx - (w - 1) * 8.f) / TS);
        ty = (int)floorf((wy - (h - 1) * 8.f) / TS);
    }
    const bool moved = tx != bm.tx || ty != bm.ty;
    bm.tx = tx;
    bm.ty = ty;
    if (!bm.demolish) bm.valid = hsCanBuild(bm.kind, tx, ty, bm.rot, &bm.why);
    if (in.rPressed || (bm.demolish && click)) {
        hsDemolish(tx, ty);
    } else if (!bm.demolish && (click || (held && moved && w == 1 && h == 1 && bm.kind != HP_GATE && bm.kind != HP_DOOR))) {
        hsPlace(bm.kind, tx, ty, bm.rot);
    }
}

// ================================================================= build menu

void Game::updateBuildMenu(float dt) {
    updateFx(dt);
    if (in.escPressed || in.buildToggle) {
        state = ST_PLAY;
        audio.play(S_CLICK);
        return;
    }
    if (in.tabPressed) bm.cat = (bm.cat + 1) % HC_COUNT, bm.scroll = 0;
    bm.scroll -= in.wheel;
}

void Game::drawBuildMenu() {
    frame(*this, "BUILD");
    text(PX + 70, PY + 7, "YOUR MATERIALS COME FROM THE PACK", C_DIM, 1, false);
    for (int c = 0; c < HC_COUNT; c++)
        if (button(PX + 8 + c * 62, PY + 20, 60, 14, HCAT_NAME[c], true, bm.cat == c ? HEX(0x7a5a2a) : UI_BTN)) bm.cat = c, bm.scroll = 0;
    std::vector<int> list;
    for (int k = 0; k < HP_COUNT; k++)
        if (HPDEF[k].cat == bm.cat) list.push_back(k);
    const int rowH = 22, vis = 5, top = PY + 40;
    bm.scroll = std::max(0, std::min((int)list.size() - vis, bm.scroll));
    int hover = -1;
    for (int r = 0; r < vis && r + bm.scroll < (int)list.size(); r++) {
        const int k = list[r + bm.scroll], y = top + r * rowH;
        const HPieceDef& D = HPDEF[k];
        const bool hov = inRect(PX + 6, y, PW - 12, rowH - 2);
        if (hov) hover = k;
        std::string lack;
        const bool ok = hsCost(k, &lack) && (k != HP_PLOT || bestTool(TT_HOE, nullptr) > 0);
        panel(PX + 6, y, PW - 12, rowH - 2, hov ? HEX(0x4a3624) : HEX(0x33241a));
        icon(A.H.pieceIcon[k], PX + 20, y + 10, 18);
        text(PX + 34, y + 3, D.name, ok ? C_TEXT : C_DIM);
        text(PX + 34, y + 12, S(D.w) + "X" + S(D.h) + (D.storage ? "  STORAGE " + S(D.storage) : "") + (D.housing ? "  HOUSES " + S(D.housing) : ""), C_DIM, 1, false);
        int cx = PX + 150;
        bool any = false;
        for (int i = 0; i < 3; i++)
            if (D.mat[i] >= 0 && D.n[i] > 0) {
                cx = chip(cx, y + 5, A.matIcon[D.mat[i]], p.mat(D.mat[i]), D.n[i]);
                any = true;
            }
        if (!any) text(cx, y + 8, k == HP_PLOT ? "FREE - NEEDS A HOE" : "FREE", C_GREEN);
        if (hov && in.uiClick) {
            in.uiClick = false;
            in.lPressed = false;
            bm.on = true;
            bm.demolish = false;
            bm.kind = k;
            bm.rot = 0;
            state = ST_PLAY;
            audio.play(S_CLICK);
            return;
        }
    }
    if ((int)list.size() > vis) text(PX + PW - 40, PY + 24, S(bm.scroll + 1) + "/" + S((int)list.size() - vis + 1), C_DIM, 1, false);
    if (hover >= 0) textC(PX + PW / 2, PY + PH - 36, HPDEF[hover].blurb, C_TEXT, 1, false);
    if (button(PX + 8, PY + PH - 22, 90, 14, "DEMOLISH MODE", true, HEX(0x7a3a2a))) {
        bm.on = true;
        bm.demolish = true;
        state = ST_PLAY;
        return;
    }
    textC(PX + PW / 2 + 40, PY + PH - 17, "TAB CATEGORY  WHEEL SCROLL  ESC CLOSE", C_DIM, 1, false);
}

// ================================================================= seed menu

void Game::updateFarmMenu(float dt) {
    updateFx(dt);
    if (in.escPressed || in.ePressed || !hsPiece(farmPieceId)) {
        state = ST_PLAY;
        farmPieceId = -1;
        audio.play(S_CLICK);
    }
}

void Game::drawFarmMenu() {
    HPiece* pc = hsPiece(farmPieceId);
    if (!pc) return;
    frame(*this, "PLANT SEEDS");
    text(PX + 80, PY + 7, pc->rich ? "FERTILE SOIL: +20% GROWTH" : "PLAIN SOIL", pc->rich ? C_GREEN : C_DIM, 1, false);
    for (int c = 0; c < CR_COUNT; c++) {
        const int y = PY + 22 + c * 20, have = p.mat(cropSeed(c));
        const CropDef& C = CROPS[c];
        const bool hov = inRect(PX + 6, y, PW - 12, 18);
        panel(PX + 6, y, PW - 12, 18, hov && have > 0 ? HEX(0x4a3624) : HEX(0x33241a));
        icon(A.matIcon[cropSeed(c)], PX + 18, y + 9, 14);
        text(PX + 30, y + 3, std::string(C.name) + " SEEDS", have > 0 ? C_TEXT : C_DIM);
        text(PX + 30, y + 11, "GROWS IN " + S((int)(C.stageSec * 4)) + "S  NEEDS " + (C.tool >= 0 ? std::string(C.tool == TT_SICKLE ? "SICKLE" : C.tool == TT_HOE ? "HOE" : "AXE") : std::string("HANDS")) +
                 (C.regrow ? "  REGROWS" : ""), C_DIM, 1, false);
        text(PX + PW - 60, y + 6, "OWN " + S(have), have > 0 ? C_GREEN : C_RED);
        if (hov && in.uiClick) {
            in.uiClick = false;
            in.lPressed = false;
            if (have > 0) {
                hsPlant(*pc, c);
                state = ST_PLAY;
                farmPieceId = -1;
                return;
            }
            msg("NO SEEDS - BUY SOME AT THE MARKET", C_RED);
            audio.play(S_ERROR, 0.6f);
        }
    }
    textC(PX + PW / 2, PY + PH - 12, "WATER THE SOIL WITH E - E / ESC CLOSE", C_DIM, 1, false);
}

// ================================================================= animal management

void Game::updateAnimalMenu(float dt) {
    updateFx(dt);
    animScroll -= in.wheel;
    if (in.escPressed || in.ePressed) {
        state = ST_PLAY;
        animSelId = -1;
        farmPieceId = -1;
        save();
        audio.play(S_CLICK);
    }
}

void Game::drawAnimalMenu() {
    frame(*this, "FARM ANIMALS");
    const HPiece* here = hsPiece(farmPieceId);
    text(PX + 90, PY + 7, S((int)hs.animals.size()) + "/" + S(H_MAX_ANIMALS) + " ANIMALS", C_DIM, 1, false);
    std::vector<int> idx;
    for (int i = 0; i < (int)hs.animals.size(); i++)
        if (!here || hs.animals[i].home == here->id) idx.push_back(i);
    if (here) {
        text(PX + 8, PY + 22, std::string(HPDEF[here->kind].name) + " " + S(hsHousingCount(here->id)) + "/" + S(HPDEF[here->kind].housing), C_TEXT);
        if (button(PX + 120, PY + 20, 70, 12, "SHOW ALL", true)) farmPieceId = -1;
    } else {
        text(PX + 8, PY + 22, "ALL ANIMALS", C_TEXT);
    }
    const int rowH = 16, vis = 8, top = PY + 36;
    animScroll = std::max(0, std::min((int)idx.size() - vis, animScroll));
    if (animSelId <= 0 && !idx.empty()) animSelId = hs.animals[idx[0]].id;
    HAnimal* sel = nullptr;
    for (auto& a : hs.animals) if (a.id == animSelId) sel = &a;
    if (idx.empty()) text(PX + 10, top + 6, here ? "NO ANIMALS LIVE HERE YET" : "NO ANIMALS - BUY SOME AT THE MARKET", C_DIM);
    for (int r = 0; r < vis && r + animScroll < (int)idx.size(); r++) {
        HAnimal& a = hs.animals[idx[r + animScroll]];
        const DomDef& D = DOMS[a.kind];
        const int y = top + r * rowH;
        const bool hov = inRect(PX + 6, y, 150, rowH - 1);
        panel(PX + 6, y, 150, rowH - 1, a.id == animSelId ? HEX(0x6a4a2a) : hov ? HEX(0x4a3624) : HEX(0x33241a));
        text(PX + 10, y + 3, a.adult(D) ? D.name : D.babyName, C_TEXT);
        bar(PX + 80, y + 3, 30, 3, a.full / 100.f, HEX(0xd89a3a));
        bar(PX + 80, y + 8, 30, 3, a.happy / 100.f, HEX(0xe0709a));
        if (a.stock > 0) icon(A.matIcon[D.product], PX + 120, y + 7, 10), text(PX + 126, y + 3, S(a.stock), C_GOLD);
        if (hov && in.uiClick) {
            in.uiClick = false;
            in.lPressed = false;
            animSelId = a.id;
            audio.play(S_CLICK, 0.5f);
        }
    }
    // the selected animal
    const int dx = PX + 166, dw = PW - 172;
    panel(dx, top, dw, 118, HEX(0x33241a));
    if (sel) {
        const DomDef& D = DOMS[sel->kind];
        const bool adult = sel->adult(D);
        ensureDomArt();
        const Spr& pv = A.Dom.get(sel->kind, 0, DS_IDLE, 0);
        if (pv.ok()) sprS(pv, dx + 28, top + 36);
        text(dx + 56, top + 4, adult ? D.name : D.babyName, C_GOLD);
        text(dx + 56, top + 13, adult ? "GROWN" : "GROWS UP IN " + S((int)(D.growSec - sel->age)) + "S", C_DIM, 1, false);
        const char* lb[3] = {"FED", "HAPPY", "HEALTH"};
        const float v[3] = {sel->full, sel->happy, sel->health};
        const u32 cl[3] = {HEX(0xd89a3a), HEX(0xe0709a), HEX(0x59b543)};
        for (int i = 0; i < 3; i++) {
            text(dx + 6, top + 50 + i * 10, lb[i], C_TEXT);
            bar(dx + 46, top + 51 + i * 10, dw - 54, 5, v[i] / 100.f, cl[i]);
        }
        std::string homeName = "FREE RANGE";
        if (const HPiece* hp = hsPiece(sel->home)) homeName = HPDEF[hp->kind].name;
        text(dx + 6, top + 82, "HOME: " + homeName, C_TEXT);
        text(dx + 6, top + 92, adult && D.product >= 0 ? std::string(D.prodName) + " WAITING: " + S(sel->stock) : "NOT PRODUCING YET", C_DIM, 1, false);
        text(dx + 6, top + 102, sel->breedCd > 0 && adult ? "BREEDS AGAIN IN " + S((int)sel->breedCd) + "S" : (adult ? "READY TO BREED" : ""), C_DIM, 1, false);
        const int by = top + 122, bw = (dw - 6) / 2;
        if (button(dx, by, bw, 14, "FEED", true, HEX(0x5d7a2e))) hsFeed(*sel);
        if (button(dx + bw + 4, by, bw, 14, "COLLECT", sel->stock > 0, HEX(0x7a5a2a))) hsCollect(*sel);
        if (button(dx, by + 17, bw, 14, "MOVE HOME", true, HEX(0x4a3a5e))) {
            // cycle: free range -> each building with room -> free range
            std::vector<int> opts{0};
            for (auto& q : hs.pieces)
                if (HPDEF[q.kind].housing > 0) opts.push_back(q.id);
            size_t cur = 0;
            for (size_t i = 0; i < opts.size(); i++) if (opts[i] == std::max(0, sel->home)) cur = i;
            for (size_t n = 1; n <= opts.size(); n++) {
                const int cand = opts[(cur + n) % opts.size()];
                if (cand == 0 || (hsHousingCount(cand) < HPDEF[hsPiece(cand)->kind].housing && !(hsPiece(cand)->kind == HP_COOP && !D.small))) {
                    hsAssign(*sel, cand);
                    break;
                }
            }
        }
        if (button(dx + bw + 4, by + 17, bw, 14, "SELL +" + S(D.price / 2), true, HEX(0x7a3a2a))) {
            p.coins += D.price / 2;
            ftext(p.pos + V2(0, -24), "+" + S(D.price / 2) + " COINS", C_GOLD);
            audio.play(S_COIN);
            const int id = sel->id;
            hs.animals.erase(std::remove_if(hs.animals.begin(), hs.animals.end(), [&](const HAnimal& q) { return q.id == id; }), hs.animals.end());
            animSelId = -1;
        }
    }
    if (button(PX + 8, PY + PH - 22, 80, 14, "COLLECT ALL", true, HEX(0x7a5a2a))) {
        int n = 0;
        for (auto& a : hs.animals) n += hsCollect(a);
        if (n == 0) msg("NOTHING TO COLLECT YET", C_DIM);
    }
    if (button(PX + 92, PY + PH - 22, 70, 14, "FEED ALL", true, HEX(0x5d7a2e))) {
        int n = 0;
        for (auto& a : hs.animals)
            if (a.full < 70.f && hsFeed(a)) n++;
        if (n == 0) msg("EVERYONE IS FED (OR YOU HAVE NO FEED)", C_DIM);
    }
    textC(PX + PW - 90, PY + PH - 9, "WHEEL SCROLL  E / ESC CLOSE", C_DIM, 1, false);
}

// ================================================================= market stall

void Game::updateMarket(float dt) {
    updateFx(dt);
    marketScroll -= in.wheel;
    if (in.tabPressed) marketTab = (marketTab + 1) % 4, marketScroll = 0;
    if (in.escPressed || in.ePressed) {
        state = ST_PLAY;
        save();
        audio.play(S_CLICK);
    }
}

void Game::drawMarket() {
    frame(*this, "HOMESTEAD MARKET");
    text(PX + PW - 70, PY + 7, S(p.coins) + " COINS", C_GOLD);
    static const char* TABS[4] = {"SEEDS", "ANIMALS", "SUPPLIES", "SELL"};
    for (int t = 0; t < 4; t++)
        if (button(PX + 8 + t * 62, PY + 20, 60, 14, TABS[t], true, marketTab == t ? HEX(0x7a5a2a) : UI_BTN)) marketTab = t, marketScroll = 0;
    struct Row { std::string name, note; int price, have, kind, id; const Spr* ic; };
    std::vector<Row> rows;
    if (marketTab == 0)
        for (int c = 0; c < CR_COUNT; c++) rows.push_back({std::string(CROPS[c].name) + " SEEDS", "GROWS IN " + S((int)(CROPS[c].stageSec * 4)) + "S", CROPS[c].seedPrice, p.mat(cropSeed(c)), 0, c, &A.matIcon[cropSeed(c)]});
    else if (marketTab == 1)
        for (int k = 0; k < DA_COUNT; k++) rows.push_back({DOMS[k].name, std::string(DOMS[k].prodName ? DOMS[k].prodName : "") + (DOMS[k].small ? "  COOP OK" : "  NEEDS BARN OR PEN"), DOMS[k].price, 0, 1, k, &A.matIcon[DOMS[k].product]});
    else if (marketTab == 2) {
        for (auto& tb : TOOL_BUY) rows.push_back({TOOLDEF[tb.tool].name, ownsTool(tb.tool) ? "YOU HAVE ONE" : "FARMING TOOL", tb.price, ownsTool(tb.tool) ? 1 : 0, 2, tb.tool, &itemIcon(itemTool(tb.tool))});
        rows.push_back({"HAY", "FEED FOR ANY ANIMAL", HAY_PRICE, p.mat(M_HAY), 3, M_HAY, &A.matIcon[M_HAY]});
    } else
        for (int m : {(int)M_BERRY, (int)M_WHEAT, (int)M_CARROT, (int)M_POTATO, (int)M_CABBAGE, (int)M_CORN, (int)M_TOMATO, (int)M_PUMPKIN, (int)M_STRAWBERRY, (int)M_EGG, (int)M_DUCKEGG, (int)M_MILK, (int)M_WOOL, (int)M_TRUFFLE, (int)M_HAY})
            if (p.mat(m) > 0) rows.push_back({MAT_NAME[m], "SELL THE WHOLE STACK", MAT_PRICE[m], p.mat(m), 4, m, &A.matIcon[m]});
    const int rowH = 18, vis = 8, top = PY + 38;
    marketScroll = std::max(0, std::min((int)rows.size() - vis, marketScroll));
    if (rows.empty()) text(PX + 10, top + 8, marketTab == 3 ? "NOTHING TO SELL - HARVEST SOMETHING FIRST" : "SOLD OUT", C_DIM);
    for (int r = 0; r < vis && r + marketScroll < (int)rows.size(); r++) {
        const Row& w = rows[r + marketScroll];
        const int y = top + r * rowH;
        panel(PX + 6, y, PW - 12, rowH - 1, HEX(0x33241a));
        icon(*w.ic, PX + 18, y + 8, 14);
        text(PX + 30, y + 2, w.name, C_TEXT);
        text(PX + 30, y + 10, w.note, C_DIM, 1, false);
        if (w.kind != 1 && w.kind != 2) text(PX + 200, y + 6, "OWN " + S(w.have), C_DIM);
        const bool sell = w.kind == 4;
        const int total = sell ? w.price * w.have : w.price;
        const bool can = sell || p.coins >= w.price;
        if (button(PX + PW - 84, y + 2, 72, 13, (sell ? "SELL +" : "BUY ") + S(total), can, sell ? HEX(0x5d7a2e) : UI_BTN)) {
            if (w.kind == 0 || w.kind == 3) {
                if (p.inv.add(itemMat(w.id), 1) < 1) msg("BAG FULL OR TOO HEAVY", C_RED);
                else p.coins -= w.price, audio.play(S_COIN, 0.6f);
            } else if (w.kind == 2) {
                if (w.have > 0) msg("YOU ALREADY HAVE ONE", C_DIM);
                else {  // a bought tool is equipped straight away
                    p.tool[TOOLDEF[w.id].type] = w.id;
                    p.coins -= w.price;
                    msg(std::string(TOOLDEF[w.id].name) + " BOUGHT AND EQUIPPED", C_GREEN);
                    audio.play(S_COIN, 0.6f);
                }
            } else if (w.kind == 1) {
                if ((int)hs.animals.size() >= H_MAX_ANIMALS) msg("THE HOMESTEAD IS FULL OF ANIMALS", C_RED);
                else {
                    float x0, y0, x1, y1;
                    HAnimal tmp;
                    hsYard(tmp, x0, y0, x1, y1);
                    hsAddAnimal(w.id, V2(frng.range(x0, x1), frng.range(y0, y1)), false, -1);
                    p.coins -= w.price;
                    msg(std::string("A ") + DOMS[w.id].name + " JOINS YOUR HOMESTEAD - ASSIGN IT A HOME", C_GREEN);
                    audio.play(S_COIN, 0.6f);
                }
            } else {
                const int n = p.inv.remove(itemMat(w.id), w.have);
                p.coins += n * w.price;
                audio.play(S_COIN);
                msg("SOLD " + S(n) + " " + w.name + " FOR " + S(n * w.price), C_GOLD);
            }
        }
    }
    textC(PX + PW / 2, PY + PH - 12, "TAB / CLICK TABS  WHEEL SCROLL  E / ESC CLOSE", C_DIM, 1, false);
}
