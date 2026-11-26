// homestead_ui.cpp - the Homestead's menus and the build-mode input: build menu, animal management.
#include <algorithm>
#include <cmath>
#include "core/game.hpp"

namespace {
constexpr int PX = 24, PY = 12, PW = 352, PH = 200;
void frame(Game& g, const std::string& title) {
    g.fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 150));
    g.panel(PX, PY, PW, PH, HEX(0x4a3524));
    g.fillRect(PX + 3, PY + 3, PW - 6, 14, UI_BG);
    g.text(PX + 8, PY + 7, title, C_GOLD);
}
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

// ================================================================= production machines (machines.hpp)

void Game::updateMachineMenu(float dt) {
    updateFx(dt);
    if (in.escPressed || in.ePressed) {
        state = ST_PLAY;
        machPieceId = -1;
        save();
        audio.play(S_CLICK);
    }
}

void Game::drawMachineMenu() {
    HPiece* pc = hsPiece(machPieceId);
    if (!pc || pc->kind < HP_MILL || pc->kind >= HP_COUNT) {
        state = ST_PLAY;
        return;
    }
    const MachineDef& M = MACHINES[pc->kind - HP_MILL];
    frame(*this, HPDEF[pc->kind].name);
    text(PX + 8, PY + 22, "PICK A RECIPE AND LOAD IT FROM YOUR PACK", C_DIM, 1, false);
    text(PX + PW - 84, PY + 22, S(pc->mq + pc->mdone) + "/" + S(MACH_MAX_BATCH) + " BATCHES", C_DIM, 1, false);
    const int rowH = 30, top = PY + 34;
    for (int r = 0; r < M.nrec; r++) {
        const MRecipe& R = M.rec[r];
        const int y = top + r * rowH;
        const bool active = pc->mrec == r && (pc->mq > 0 || pc->mdone > 0);
        int can = 99;
        for (int i = 0; i < 2; i++)
            if (R.in[i] >= 0) can = std::min(can, p.inv.count(itemMat(R.in[i])) / R.n[i]);
        panel(PX + 6, y, PW - 12, rowH - 2, active ? HEX(0x4a3a1e) : HEX(0x33241a));
        icon(itemIcon(machOutItem(R)), PX + 20, y + 13, 18);
        text(PX + 34, y + 4, S(R.outN) + " " + item(machOutItem(R)).name, can > 0 ? C_TEXT : C_DIM);
        std::string from = MAT_NAME[R.in[0]];
        if (R.in[1] >= 0) from += std::string(" + ") + MAT_NAME[R.in[1]];
        text(PX + 34, y + 15, from + "  " + S((int)R.sec) + "S", C_DIM, 1, false);
        int cx = PX + 196;
        for (int i = 0; i < 2; i++)
            if (R.in[i] >= 0) cx = chip(cx, y + 7, A.matIcon[R.in[i]], p.mat(R.in[i]), R.n[i]);
        const bool free = !((pc->mq > 0 || pc->mdone > 0) && pc->mrec != r) && pc->mq + pc->mdone < MACH_MAX_BATCH;
        if (button(PX + PW - 100, y + 7, 40, 13, "LOAD", free && can > 0)) hsMachineLoad(*pc, r, 1);
        if (button(PX + PW - 56, y + 7, 40, 13, "MAX", free && can > 0)) hsMachineLoad(*pc, r, MACH_MAX_BATCH);
    }
    const int sy = top + 4 * rowH + 2;
    panel(PX + 6, sy, PW - 12, 34, HEX(0x2a1c14));
    if (pc->mrec >= 0 && pc->mrec < M.nrec && (pc->mq > 0 || pc->mdone > 0)) {
        const MRecipe& R = M.rec[pc->mrec];
        text(PX + 12, sy + 4, std::string("MAKING ") + item(machOutItem(R)).name + "   LEFT " + S(pc->mq) + "   READY " + S(pc->mdone * R.outN), C_TEXT);
        const float f = pc->mq > 0 ? std::max(0.f, std::min(1.f, pc->mt / R.sec)) : 1.f;
        fillRect(PX + 12, sy + 16, 200, 6, HEX(0x1c1410));
        fillRect(PX + 12, sy + 16, (int)(200 * f), 6, pc->mq > 0 ? HEX(0xe0a040) : HEX(0x6cc04a));
        text(PX + 12, sy + 25, pc->mq > 0 ? "WORKS WHILE YOU ARE AT THE HOMESTEAD" : "ALL DONE - COLLECT IT", C_DIM, 1, false);
    } else {
        text(PX + 12, sy + 12, "IDLE - LOAD A RECIPE", C_DIM);
    }
    if (button(PX + PW - 106, sy + 10, 90, 14, "COLLECT", pc->mdone > 0, HEX(0x3a6a2a))) hsMachineCollect(*pc);
    textC(PX + PW / 2, PY + PH - 12, "E OR ESC: CLOSE", C_DIM, 1, false);
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
    if (idx.empty()) text(PX + 10, top + 6, here ? "NO ANIMALS LIVE HERE YET" : "NO ANIMALS - BUY SOME AT THE HOMESTEAD MARKET", C_DIM);
    for (int r = 0; r < vis && r + animScroll < (int)idx.size(); r++) {
        HAnimal& a = hs.animals[idx[r + animScroll]];
        const DomDef& D = DOMS[a.kind];
        const int y = top + r * rowH;
        const bool hov = inRect(PX + 6, y, 150, rowH - 1);
        panel(PX + 6, y, 150, rowH - 1, a.id == animSelId ? HEX(0x6a4a2a) : hov ? HEX(0x4a3624) : HEX(0x33241a));
        text(PX + 10, y + 3, a.label(D), C_TEXT);
        bar(PX + 80, y + 3, 30, 3, a.full / 100.f, HEX(0xd89a3a));
        bar(PX + 80, y + 8, 30, 3, a.happy / 100.f, HEX(0xe0709a));
        if (a.stock > 0 && D.product >= 0) icon(A.matIcon[D.product], PX + 120, y + 7, 10), text(PX + 126, y + 3, S(a.stock), C_GOLD);
        else if (a.saddled) text(PX + 118, y + 3, "RIDE", C_GREEN, 1, false);
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
        Spr pv;
        if (sel->kind != DA_MOUNT) pv = A.Dom.get(sel->kind, 0, DS_IDLE, 0);  // (the mount has no farm-animal art: it uses its wild species' art)
        else {
            if (ren) ensureRichBank(ren, A, sel->species, 0, sel->tier);
            if (const RichAnimal* rb = A.rich[sel->species].get()) pv = rb->s[2 * sel->tier][F_E][AN_IDLE][0];
            if (pv.ok() && !adult) pv.sc *= 0.5f + 0.5f * std::min(1.f, sel->age / sel->growSec(D));
        }
        if (pv.ok()) sprS(pv, dx + 28, top + 36);
        text(dx + 56, top + 4, sel->label(D), sel->kind == DA_MOUNT ? TIER[sel->tier].col : C_GOLD);
        text(dx + 56, top + 13, adult ? "GROWN" : "GROWS UP IN " + S((int)(sel->growSec(D) - sel->age)) + "S", C_DIM, 1, false);
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
        text(dx + 6, top + 92, sel->kind == DA_MOUNT ? (!adult ? "TOO YOUNG TO RIDE" : sel->saddled ? "SADDLED - PRESS V TO RIDE" : "SADDLE IT TO RIDE (V)") : adult && D.product >= 0 ? std::string(D.prodName) + " WAITING: " + S(sel->stock) : "NOT PRODUCING YET", C_DIM, 1, false);
        text(dx + 6, top + 102, sel->kind == DA_MOUNT ? (sel->companion ? "COMPANION - BOND LEVEL " + S(bondLevel(sel->bond) + 1) : sel->breedCd > 0 && adult ? "BREEDS AGAIN IN " + S((int)sel->breedCd) + "S" : (adult ? "READY TO BREED WITH ITS OWN KIND" : std::string(""))) : sel->breedCd > 0 && adult ? "BREEDS AGAIN IN " + S((int)sel->breedCd) + "S" : (adult ? "READY TO BREED" : ""), C_DIM, 1, false);
        const int by = top + 122, bw = (dw - 6) / 2;
        if (button(dx, by, bw, 14, "FEED", true, HEX(0x5d7a2e))) hsFeed(*sel);
        if (sel->kind == DA_MOUNT) {  // a mount has nothing to collect: that button makes it the companion that walks with the hunter
            if (button(dx + bw + 4, by, bw, 14, sel->companion ? "DISMISS PARTNER" : "COMPANION", adult, HEX(0x2e5a6a))) {
                const bool on = !sel->companion;
                for (auto& o : hs.animals) o.companion = false;  // one companion at a time
                sel->companion = on;
                if (on) sel->saddled = false;  // a partner walks beside you; a mount carries you - never both
                msg(on ? "COMPANION CHOSEN - IT WALKS WITH YOU OUTSIDE" : "COMPANION DISMISSED", C_GREEN);
            }
        } else if (button(dx + bw + 4, by, bw, 14, "COLLECT", sel->stock > 0, HEX(0x7a5a2a))) hsCollect(*sel);
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
        if (sel->kind == DA_MOUNT) {
            if (button(dx + bw + 4, by + 17, bw, 14, sel->saddled ? "UNSADDLE" : "SADDLE", adult, HEX(0x6a4a2a))) {
                if (!sel->saddled) for (auto& o : hs.animals) o.saddled = false;  // one saddled mount at a time
                sel->saddled = !sel->saddled;
                if (sel->saddled) sel->companion = false;
                msg(sel->saddled ? "SADDLED - PRESS V TO RIDE" : "SADDLE REMOVED", C_GREEN);
            }
        } else if (button(dx + bw + 4, by + 17, bw, 14, "SELL +" + S(D.price / 2), true, HEX(0x7a3a2a))) {
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
