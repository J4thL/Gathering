// gathering.cpp - harvesting trees, rocks, ore and plants.
//
// What can be harvested and what it gives is data (resources.hpp). This file is the behaviour:
//   * finding the resource the hunter is standing next to (reach, line of sight, tool requirement)
//   * the hold-E swing loop: every swing lands a hit, the last hit finishes the resource and pays out
//   * depletion and regrowth: stump / rubble / stub  ->  sprout  ->  full again (and solid again for trees and rocks)
//   * keeping the pack safe: a resource only finishes when everything it gives fits in the bag
// Gathering uses its own random stream (grng), so it never disturbs the hunting / wildlife sequences.
#include <algorithm>
#include <map>
#include <sstream>
#include "game.hpp"

namespace {
constexpr int NODE_FLAG = 1 << 28;
inline int enc(bool node, int idx) { return (node ? NODE_FLAG : 0) | idx; }
inline bool isNodeId(int id) { return (id & NODE_FLAG) != 0; }
inline int idxOf(int id) { return id & (NODE_FLAG - 1); }
// ore and cactus nodes are solid tiles (placed that way by worldgen); they stay solid when finished
inline bool nodeIsSolid(int kind) { return kind == RN_CACTUS || kind == RN_ORE || kind == RN_COPPER || kind == RN_GOLD; }
}  // namespace

// ------------------------------------------------------------------ state
void Game::initHarvest() {
    propSt.assign(world.props.size(), HState());
    nodeSt.assign(world.nodes.size(), HState());
    propRsId.assign(world.props.size(), -1);
    for (size_t i = 0; i < world.props.size(); i++) {
        int r = propRes(world.props[i].kind);
        if (r >= 0 && protectedTile((int)(world.props[i].x / TS), (int)(world.props[i].y / TS), world.mapId)) r = -1;
        propRsId[i] = (int8_t)r;
    }
    hActive.clear();
    hvKey = -1;
}

void Game::resetHarvest() {
    for (int id : hActive) {
        if (isNodeId(id)) continue;
        const Prop& pr = world.props[idxOf(id)];
        world.solid[(int)(pr.y / TS)][(int)(pr.x / TS)] = true;  // a felled tree / broken rock is standing again
    }
    for (auto& s : propSt) s = HState();
    for (auto& s : nodeSt) s = HState();
    hActive.clear();
    hvKey = -1;
}

bool Game::harvestUp(bool node, int idx) const {
    const HState& s = hstate(node, idx);
    return !s.gone && s.fade <= 0;
}

int Game::bestTool(int type, int* toolIdx) const {
    int best = 0, bi = -1;
    for (int k = 0; k < TL_COUNT; k++)
        if (TOOLDEF[k].type == type && TOOLDEF[k].tier > best && p.inv.count(itemTool(k)) > 0) best = TOOLDEF[k].tier, bi = k;
    if (toolIdx) *toolIdx = bi;
    return best;
}

// A straight line from a to b must not cross a solid tile (other than the resource's own tile): no gathering through walls or trees.
bool Game::lineClear(V2 a, V2 b, int ignoreTx, int ignoreTy) const {
    V2 d = b - a;
    float len = d.len();
    int steps = std::max(1, (int)ceilf(len / 3.f));
    for (int i = 1; i < steps; i++) {
        float f = (float)i / steps;
        int tx = (int)floorf((a.x + d.x * f) / TS), ty = (int)floorf((a.y + d.y * f) / TS);
        if (tx == ignoreTx && ty == ignoreTy) continue;
        if (world.solidTile(tx, ty)) return false;
    }
    return true;
}

bool Game::findHarvest(HTarget& out) const {
    out = HTarget();
    const float R = 34.f;
    auto yLess = [](const auto& e, float y) { return e.y < y; };
    auto consider = [&](bool node, int idx, V2 c, float reach, int tx, int ty, int res) {
        float d = dist(p.pos, c);
        if (d > reach || (out.valid && d >= out.dist)) return;
        if (!lineClear(p.pos + V2(0, -3), c, tx, ty)) return;
        out.valid = true;
        out.node = node;
        out.idx = idx;
        out.res = res;
        out.pos = c;
        out.dist = d;
    };
    const auto& N = world.nodes;
    for (auto it = std::lower_bound(N.begin(), N.end(), p.pos.y - R, yLess); it != N.end() && it->y <= p.pos.y + R; ++it) {
        int idx = (int)(it - N.begin());
        if (!harvestUp(true, idx)) continue;
        consider(true, idx, V2(it->x, it->y - 4), HARVEST_REACH_NODE, (int)(it->x / TS), (int)(it->y / TS), nodeRes(it->kind));
    }
    const auto& P = world.props;
    for (auto it = std::lower_bound(P.begin(), P.end(), p.pos.y - R, yLess); it != P.end() && it->y <= p.pos.y + R + 16; ++it) {
        int idx = (int)(it - P.begin());
        if (propRsId[idx] < 0 || !harvestUp(false, idx)) continue;
        consider(false, idx, V2(it->x, it->y - 6), HARVEST_REACH_PROP, (int)(it->x / TS), (int)(it->y / TS), propRsId[idx]);
    }
    return out.valid;
}

std::string Game::harvestVerb(int res) const {
    switch (RES[res].cls) {
        case RCL_TREE: return RES[res].tool == TT_AXE && RES[res].minTier > 0 ? "CHOP" : "GATHER";
        case RCL_ROCK:
        case RCL_ORE: return "MINE";
        default: return "PICK";
    }
}

// ------------------------------------------------------------------ effects
void Game::harvestChips(V2 at, u32 col, int n, float power) {
    for (int i = 0; i < n; i++) {
        float a = grng.range(0.f, 6.2832f), sp = grng.range(18.f, 70.f) * power;
        u32 c = grng.chance(0.3f) ? shadeC(col, 1.3f) : (grng.chance(0.4f) ? shadeC(col, 0.7f) : col);
        parts.push_back({at.x + grng.range(-3, 3), at.y + 2, grng.range(2, 9), cosf(a) * sp, sinf(a) * sp * 0.5f, grng.range(30, 90) * power, 0.7f, 0.7f, c,
                         grng.chance(0.25f) ? 2 : 1, true});
    }
}

// ------------------------------------------------------------------ the swing loop
// Called every frame while the hunter holds E at a resource they are allowed to harvest.
void Game::harvestStep(const HTarget& t, float dt) {
    if (!t.valid || !harvestUp(t.node, t.idx)) return;  // a depleted or regrowing resource gives nothing (no duplicate rewards)
    const ResDef& R = RES[t.res];
    int toolIdx = -1;
    int tier = R.tool >= 0 ? bestTool(R.tool, &toolIdx) : 0;
    float speed = R.tool < 0 ? 1.f : (tier > 0 ? TOOLDEF[toolIdx].speed : HAND_SPEED);
    if (R.cls == RCL_TREE) speed *= 1.f + skillBonus(SK_WOODCUTTING);
    else if (R.cls == RCL_ROCK || R.cls == RCL_ORE) speed *= 1.f + skillBonus(SK_MINING);
    if (t.key() != hvKey) {  // a different resource: the swing starts over
        hvKey = t.key();
        p.gatherT = 0;
    }
    p.gathering = true;
    p.gatherT += dt * speed;
    if (p.gatherT < R.swing) return;
    p.gatherT = 0;
    HState& s = hstate(t.node, t.idx);
    if (s.done + 1 >= R.hits) {
        harvestFinish(t);
        return;
    }
    s.done++;
    s.t = 0;
    s.shake = 0.22f;
    if (std::find(hActive.begin(), hActive.end(), enc(t.node, t.idx)) == hActive.end()) hActive.push_back(enc(t.node, t.idx));
    harvestChips(t.pos + V2(0, -4), HEX(R.chip), R.cls == RCL_PLANT ? 3 : 6, 1.f);
    shakeAmt = std::max(shakeAmt, R.cls == RCL_TREE ? 1.2f : 0.8f);
    audio.play(R.cls == RCL_TREE ? S_CHOP : R.cls == RCL_PLANT ? S_RUSTLE : S_MINE, 0.7f);
}

void Game::harvestFinish(const HTarget& t) {
    const ResDef& R = RES[t.res];
    int toolIdx = -1;
    int tier = R.tool >= 0 ? bestTool(R.tool, &toolIdx) : 0;
    // roll what it gives
    int mat[3] = {-1, -1, -1}, n[3] = {0, 0, 0};
    for (int k = 0; k < 3; k++) {
        const ResYield& y = R.y[k];
        if (y.mat < 0 || tier < y.minTier || !grng.chance(y.chance)) continue;
        mat[k] = y.mat;
        n[k] = grng.irange(y.lo, y.hi);
    }
    bool bonus = false;
    if (n[0] > 0 && R.cls == RCL_PLANT && grng.chance(skillBonus(SK_FORAGING))) {  // a keen forager finds an extra plant
        n[0]++;
        bonus = true;
    }
    if (n[0] > 0 && toolIdx >= 0 && grng.chance(TOOLDEF[toolIdx].bonus)) {
        n[0]++;
        bonus = true;
    }
    // everything must fit: a resource is never finished into a full bag (nothing is lost, nothing is wasted)
    Inventory test = p.inv;
    for (int k = 0; k < 3; k++)
        if (n[k] > 0 && test.add(itemMat(mat[k]), n[k]) < n[k]) {
            notifyBlocked(itemMat(mat[k]), n[k], &test);
            return;
        }
    int lineY = 0;
    for (int k = 0; k < 3; k++) {
        if (n[k] <= 0) continue;
        p.inv.add(itemMat(mat[k]), n[k]);
        questEvent(Q_GATHER, mat[k], n[k]);
        ftext(t.pos + V2(grng.range(-5, 5), -12 - lineY), "+" + S(n[k]) + " " + MAT_NAME[mat[k]], (k == 0 && bonus) ? C_GOLD : (k == 0 ? C_GREEN : C_BLUE), 1.4f);
        lineY += 8;
    }
    if (bonus) ftext(t.pos + V2(0, -12 - lineY), "BONUS!", C_GOLD, 1.2f);
    // skill XP: only here, after the pack check passed and the yield was delivered; harder resources pay more
    grantXp(R.cls == RCL_TREE ? SK_WOODCUTTING : R.cls == RCL_PLANT ? SK_FORAGING : SK_MINING, 2 + R.hits * 2 + R.minTier * 4);
    // finished: depleted state + regrow timer
    HState& s = hstate(t.node, t.idx);
    s.gone = true;
    s.done = 0;
    s.fade = 0;
    s.t = R.respawn * grng.range(0.85f, 1.15f);
    s.shake = 0;
    if (std::find(hActive.begin(), hActive.end(), enc(t.node, t.idx)) == hActive.end()) hActive.push_back(enc(t.node, t.idx));
    if (!t.node) {  // a felled tree / broken rock no longer blocks the way
        const Prop& pr = world.props[t.idx];
        world.solid[(int)(pr.y / TS)][(int)(pr.x / TS)] = false;
    }
    harvestChips(t.pos + V2(0, -4), HEX(R.chip), R.cls == RCL_PLANT ? 8 : 16, 1.5f);
    shakeAmt = std::max(shakeAmt, R.cls == RCL_TREE ? 2.4f : 1.4f);
    audio.play(R.cls == RCL_TREE ? S_FELL : R.cls == RCL_PLANT ? S_PICKUP : S_MINE, 0.8f);
    if (R.cls != RCL_PLANT) audio.play(S_PICKUP, 0.35f);
    // thorns and poison
    const bool wrapsSafe = R.safeArmor >= 2 ? p.has(CG_VENOMWRAPS) : (p.has(CG_DESERTWRAPS) || p.has(CG_VENOMWRAPS));  // wraps shield hands from thorns / poison sap
    if (R.hurt > 0 && p.up[U_ARMOR] < R.safeArmor && !wrapsSafe && !p.dead) {
        p.hp = std::max(1.f, p.hp - R.hurt);
        p.sinceHurt = 0;
        ftext(p.pos + V2(0, -26), R.safeArmor >= 2 ? "POISON! -" + S(R.hurt) : "THORNS! -" + S(R.hurt), HEX(0x9acd6a), 1.2f);
        audio.play(S_HURT, 0.35f);
    }
    p.gatherT = 0;
}

// ------------------------------------------------------------------ regrowth
void Game::updateHarvest(float dt) {
    if (hvMsgT > 0) hvMsgT -= dt;
    for (size_t i = 0; i < hActive.size();) {
        const int id = hActive[i];
        const bool node = isNodeId(id);
        const int idx = idxOf(id);
        HState& s = hstate(node, idx);
        bool keep = true;
        if (s.shake > 0) s.shake = std::max(0.f, s.shake - dt);
        if (s.gone) {
            s.t -= dt;
            if (s.t <= 0) {  // a sprout appears; the resource itself is not usable (or solid) until it has grown
                s.gone = false;
                s.fade = REGROW_FADE;
            }
        } else if (s.fade > 0) {
            s.fade -= dt;
            if (s.fade <= 0) {
                bool blocked = false;
                if (!node) {  // a tree only grows back when nobody is standing on its tile and nothing was built there
                    const Prop& pr = world.props[idx];
                    const int tx = (int)(pr.x / TS), ty = (int)(pr.y / TS);
                    V2 c(tx * TS + 8.f, ty * TS + 8.f);
                    blocked = world.solid[ty][tx] || dist(p.pos, c) < 13.f;
                    for (auto& a : animals)
                        if (!a.dead && dist(a.pos, c) < 12.f) blocked = true;
                    if (!blocked) world.solid[ty][tx] = true;
                }
                if (blocked) s.fade = 1.0f;  // try again in a second
                else {
                    s.fade = 0;
                    s.done = 0;
                    s.t = 0;
                    keep = false;
                }
            }
        } else {  // damaged but not finished: it recovers if left alone
            s.t += dt;
            if (s.t > HARVEST_HEAL_AFTER) {
                s.done = 0;
                s.t = 0;
                keep = false;
            }
        }
        if (keep) i++;
        else hActive.erase(hActive.begin() + i);
    }
}

// ------------------------------------------------------------------ saving: only the finished (waiting to regrow) resources
// "hv <n|p> <tile x> <tile y> <res> <seconds left>": found again by tile and kind, so it survives a change in generation order.
void Game::harvestSave(std::ostream& f) const {
    for (int id : hActive) {
        const bool node = isNodeId(id);
        const int idx = idxOf(id);
        const HState& s = hstate(node, idx);
        if (!s.gone) continue;
        float x = node ? world.nodes[idx].x : world.props[idx].x, y = node ? world.nodes[idx].y : world.props[idx].y;
        int res = node ? nodeRes(world.nodes[idx].kind) : propRsId[idx];
        f << "hv " << (node ? 'n' : 'p') << ' ' << (int)(x / TS) << ' ' << (int)(y / TS) << ' ' << res << ' ' << (int)s.t << "\n";
    }
}

void Game::harvestLoad(const std::vector<std::string>& lines) {
    if (lines.empty()) return;
    std::map<long, int> nodeAt, propAt;
    auto key = [](int tx, int ty, int res) { return ((long)tx * 4096 + ty) * 64 + res; };
    for (size_t i = 0; i < world.nodes.size(); i++)
        nodeAt[key((int)(world.nodes[i].x / TS), (int)(world.nodes[i].y / TS), nodeRes(world.nodes[i].kind))] = (int)i;
    for (size_t i = 0; i < world.props.size(); i++)
        if (propRsId[i] >= 0) propAt[key((int)(world.props[i].x / TS), (int)(world.props[i].y / TS), propRsId[i])] = (int)i;
    for (auto& ln : lines) {
        std::istringstream ss(ln);
        std::string tag;
        char kind = 'n';
        int tx = 0, ty = 0, res = 0, secs = 0;
        ss >> tag >> kind >> tx >> ty >> res >> secs;
        if (tag != "hv" || !ss) continue;
        const bool node = kind == 'n';
        auto& m = node ? nodeAt : propAt;
        auto it = m.find(key(tx, ty, res));
        if (it == m.end()) continue;  // not in this world any more: ignore
        HState& s = hstate(node, it->second);
        s.gone = true;
        s.t = (float)std::max(1, std::min(3600, secs));
        if (!node) {
            const Prop& pr = world.props[it->second];
            world.solid[(int)(pr.y / TS)][(int)(pr.x / TS)] = false;
        }
        hActive.push_back(enc(node, it->second));
    }
}

// ------------------------------------------------------------------ HUD: name plate, yields, hits left, progress, tool hints
void Game::drawHarvestPrompt() {
    HTarget t;
    if (!findHarvest(t)) return;
    const ResDef& R = RES[t.res];
    int toolIdx = -1;
    int tier = R.tool >= 0 ? bestTool(R.tool, &toolIdx) : 0;
    const bool ok = R.tool < 0 || tier >= R.minTier;
    const HState& s = hstate(t.node, t.idx);
    const int sx = (int)t.pos.x - camX, sy = (int)t.pos.y - camY;

    // name plate: name, what it gives, hits left
    std::string yields;
    int icons[3], nIcons = 0;
    for (int k = 0; k < 3; k++)
        if (R.y[k].mat >= 0 && tier >= R.y[k].minTier && (k == 0 || R.y[k].mat != R.y[k - 1].mat)) icons[nIcons++] = R.y[k].mat;
    int pw = std::max(textW(R.name) + 10, nIcons * 12 + R.hits * 5 + 12);
    int px = sx - pw / 2, py = sy - 58;
    panel(px, py, pw, 21, HEX(0x3a2a1a));
    textC(sx, py + 3, R.name, C_TEXT, 1, true);
    for (int k = 0; k < nIcons; k++) icon(A.matIcon[icons[k]], px + 9 + k * 12, py + 15, 9, ok ? 1.f : 0.5f);
    for (int h = 0; h < R.hits; h++) {
        bool left = h >= s.done;
        fillRect(px + pw - 5 - (R.hits - h) * 5 + 1, py + 13, 4, 5, left ? C_GREEN : HEX(0x4a3524));
    }
    if (!ok) {
        std::string need = std::string("NEED ") + TOOLDEF[R.tool * 4 + R.minTier - 1].name;
        int w = 12 + textW(need);
        panel(sx - w / 2, sy - 36, w, 13, HEX(0x4a2020));
        textC(sx, sy - 33, need, C_RED, 1, true);
    } else if (p.gathering && hvKey == t.key()) {
        float speed = R.tool < 0 ? 1.f : (tier > 0 ? TOOLDEF[toolIdx].speed : HAND_SPEED);
        (void)speed;
        bar(sx - 14, sy - 30, 28, 4, p.gatherT / R.swing, C_GREEN);
    } else {
        std::string label = "HOLD TO " + harvestVerb(t.res);
        if (R.tool >= 0 && tier == 0) label += " BY HAND";
        drawPrompt(t.pos.x, t.pos.y - 22, "E", label);
    }
}
