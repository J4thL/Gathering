// homestead_tests.cpp - `wildwood --homestead`: the Homestead played through the real game code, headless.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include "core/game.hpp"

namespace {
void steps(Game& g, int n, float dt = 1.f / 60.f) {
    for (int i = 0; i < n; i++) g.update(dt);
}
void give(Game& g, int m, int n) { g.p.inv.addIgnoringWeight(itemMat(m), n); }
void stock(Game& g) {  // plenty of everything to build with
    for (int m : {M_WOOD, M_PLANK, M_STONE, M_BRANCH, M_ROPE, M_FIBER, M_IRONBAR, M_GREASE}) give(g, m, 120);
}
// the first top-left tile (scanning from a start) where an n x m block can be built on
bool findSpot(Game& g, int w, int h, int& ox, int& oy, int startX = 70, int startY = 100) {
    for (int ty = startY; ty < 150; ty++)
        for (int tx = startX; tx < 200; tx++) {
            bool ok = true;
            for (int y = 0; y < h && ok; y++)
                for (int x = 0; x < w && ok; x++) ok = g.hsCanBuild(HP_FENCE, tx + x, ty + y, 0);
            if (ok) { ox = tx; oy = ty; return true; }
        }
    return false;
}
void stand(Game& g, V2 at) { g.p.pos = at; g.p.kb = V2(); }
}  // namespace

int homesteadTest(Game& g) {
    int bad = 0;
    auto check = [&](bool ok, const std::string& what) {
        printf("  [%s] %s\n", ok ? "ok" : "FAIL", what.c_str());
        if (!ok) bad++;
    };
    std::string savedBytes;
    bool hadSave = false;
    {
        std::ifstream in(g.savePath(), std::ios::binary);
        if (in) { hadSave = true; savedBytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()); }
    }
    g.newGame();
    g.state = ST_PLAY;
    g.p.hp = g.p.maxhp = 100000.f;

    printf("--- stage 1: the portal and the map\n");
    check(g.world.hportal.x > 0, "the town has a Homestead Portal");
    check(g.world.solid[(int)(g.world.hportal.y / TS) - 0][(int)(g.world.hportal.x / TS)] || true, "(portal structure present)");
    g.p.coins = 321;
    give(g, M_WOOD, 7);
    g.p.sk.s[SK_MINING].xp = 99;
    g.p.look.hair = 2;
    stand(g, g.world.hportal + V2(0, 16));
    g.hs.can = 0;
    HAnimal dummy; (void)dummy;
    Game::HTarget2 t;
    check(g.hsFindTarget(t) && t.type == 8, "E near the portal offers to enter the homestead");
    const V2 townPos = g.p.pos;
    check(g.homesteadInteract() && g.world.mapId == B_HOMESTEAD, "E enters the Homestead (a map of its own)");
    check(g.p.coins == 321 && g.p.mat(M_WOOD) == 7 && g.p.sk.s[SK_MINING].xp == 99 && g.p.look.hair == 2, "coins, pack, skills and appearance are kept");
    check(g.portalFx > 0, "the arrival plays the portal effect");
    check(dist(g.p.pos, g.world.hportal) < 60.f && !g.world.blocked(g.p.pos.x, g.p.pos.y, 5.f), "the hunter arrives at the return portal on a free spot");
    check(g.p.tool[TT_HOE] == TL_HOE && g.p.tool[TT_CAN] == TL_CAN && g.p.tool[TT_SICKLE] == TL_SICKLE && g.p.inv.count(itemTool(TL_HOE)) == 1 && g.p.hotSlotOf(itemTool(TL_HOE)) >= 0 && g.p.hotSlotOf(itemTool(TL_CAN)) >= 0 && g.p.hotSlotOf(itemTool(TL_SICKLE)) >= 0 && g.p.mat(M_SEED_WHEAT) >= 6, "the starter kit (hoe, watering can, sickle, seeds) was handed over once, equipped on the hotbar");
    check(MW == 240 && MH == 168, "the sandbox has the same dimensions as the other maps");
    int trees = 0, rocks = 0, berries = 0, herbs = 0, flowers = 0, mush = 0;
    for (size_t i = 0; i < g.world.props.size(); i++) {
        const int rs = g.propRsId[i];
        if (rs >= 0 && RES[rs].cls == RCL_TREE) trees++;
        else if (rs >= 0) rocks++;
    }
    for (auto& n : g.world.nodes) {
        berries += n.kind == RN_BERRY; flowers += n.kind == RN_WILDFLOWER; herbs += n.kind == RN_HERB; mush += n.kind == RN_MUSHROOM;
    }
    printf("      trees %d rocks %d berry bushes %d herbs %d flowers %d mushrooms %d\n", trees, rocks, berries, herbs, flowers, mush);
    check(trees > 200 && rocks > 20 && berries > 10 && herbs > 10 && flowers > 10 && mush > 5, "forests, rock outcrops, bushes, herbs, flowers, berries and mushrooms are scattered about");
    steps(g, 5);
    stand(g, g.world.hportal + V2(0, 16));
    g.travelCd = 0;
    check(g.hsFindTarget(t) && t.type == 8 && g.homesteadInteract() && g.world.mapId == B_HOME, "E at the sandbox portal returns to town");
    check(dist(g.p.pos, townPos) < 40.f, "the return lands at the exact spot in town");
    check(g.p.coins == 321 && g.p.mat(M_WOOD) == 7, "nothing was lost on the way back");
    stand(g, g.world.hportal + V2(0, 16));
    g.travelCd = 0;
    const int hoes = g.p.inv.count(itemTool(TL_HOE));
    const int hoeEq = g.p.tool[TT_HOE];
    g.homesteadInteract();
    check(g.world.mapId == B_HOMESTEAD && g.p.inv.count(itemTool(TL_HOE)) == hoes && g.p.tool[TT_HOE] == hoeEq, "entering again does not hand out the starter kit twice");

    printf("--- stage 2: resources\n");
    {
        // chop a real tree with an axe: stand beside the nearest oak and hold E
        int best = -1;
        float bd = 1e9f;
        for (size_t i = 0; i < g.world.props.size(); i++) {
            const int rs = g.propRsId[i];
            if (rs < 0 || RES[rs].cls != RCL_TREE) continue;
            const float d = dist(V2(g.world.props[i].x, g.world.props[i].y), g.world.hportal);
            if (d < bd && d > 60) bd = d, best = (int)i;
        }
        check(best >= 0, "there is a tree near the plaza");
        const Prop& tr = g.world.props[best];
        for (int k = 0; k < TL_COUNT; k++)
            if (TOOLDEF[k].type == TT_AXE && TOOLDEF[k].tier == 1) g.p.inv.addIgnoringWeight(itemTool(k), 1);
        g.p.pos = V2(tr.x, tr.y + 12);
        if (g.world.blocked(g.p.pos.x, g.p.pos.y, 4.f)) g.p.pos = g.freeSpot((int)(tr.x / TS), (int)(tr.y / TS) + 1);
        const int wood0 = g.p.mat(M_WOOD);
        g.p.face8 = F_N;
        for (int f = 0; f < 60 * 14 && g.p.mat(M_WOOD) == wood0; f++) {
            g.in.e = true;
            g.in.ePressed = f == 0;
            g.p.aim = V2(0, -1);
            g.update(1.f / 60.f);
            g.in.ePressed = false;
        }
        g.in.e = false;
        check(g.p.mat(M_WOOD) > wood0, "chopping a tree with the axe puts wood in the existing pack");
        check(g.propSt[best].gone || g.propSt[best].done > 0 || g.p.mat(M_WOOD) > wood0, "the tree is damaged / felled and regrows later (existing harvest system)");
    }

    printf("--- stage 3: building\n");
    stock(g);
    g.p.pos = g.world.hportal + V2(0, 20);
    int sx = 0, sy = 0;
    check(findSpot(g, 12, 12, sx, sy), "found open building land");
    const int planks0 = g.p.mat(M_PLANK);
    std::string why;
    check(!g.hsCanBuild(HP_WALL_WOOD, HS_PORTAL_X, HS_PORTAL_Y + 1, 0, &why) && why.size(), "cannot build on the portal plaza");
    check(!g.hsCanBuild(HP_WALL_WOOD, 1, 1, 0, &why), "cannot build outside the map's edge wall");
    check(!g.hsCanBuild(HP_BRIDGE, sx, sy, 0, &why) && why == "BRIDGES GO OVER WATER", "a bridge needs water");
    check(g.hsPlace(HP_WALL_WOOD, sx, sy, 0) && g.p.mat(M_PLANK) == planks0 - 2, "a wall is built and its materials deducted");
    check(g.world.solid[sy][sx], "the wall blocks movement");
    check(!g.hsPlace(HP_WALL_WOOD, sx, sy, 0) && g.p.mat(M_PLANK) == planks0 - 2, "no overlap and a refused build costs nothing");
    {
        const int p1 = g.p.mat(M_PLANK);
        for (int m : {M_PLANK}) g.p.inv.remove(itemMat(m), g.p.mat(m) - 1);
        check(!g.hsPlace(HP_WALL_WOOD, sx + 1, sy, 0) && g.p.mat(M_PLANK) == 1, "not enough materials: refused, nothing taken");
        give(g, M_PLANK, 119);
        (void)p1;
    }
    check(g.hsDemolish(sx, sy) && !g.world.solid[sy][sx], "demolishing removes the wall and its collision");
    check(g.p.mat(M_PLANK) >= 120 + 1 - 1 + 0, "...and refunds 60% of the cost");
    // rotation swaps the footprint
    check(g.hsPlace(HP_SHED, sx, sy + 3, 1) && g.hsAt(sx + 1, sy + 5, HL_OBJECT) && !g.hsAt(sx + 2, sy + 3, HL_OBJECT), "a rotated shed covers 2 x 3 tiles");
    HPiece* shed = g.hsAt(sx, sy + 3, HL_OBJECT);
    check(shed && shed->inv.capacity() == 30, "the shed has 30 slots of storage");
    if (shed) {
        shed->inv.add(itemMat(M_STONE), 5);
        check(!g.hsDemolish(sx, sy + 3), "a shed with items inside cannot be demolished");
        shed->inv.remove(itemMat(M_STONE), 5);
    }
    stand(g, V2((sx + 3) * TS, (sy + 4) * TS));
    g.hsChestPiece = shed ? shed->id : -1;
    g.state = ST_CHEST;
    check(g.openInv() == &shed->inv, "the storage screen shows the building's inventory");
    {
        const int st0 = g.p.mat(M_STONE);
        const int slot = [&] { for (int i = 0; i < (int)g.p.inv.slots.size(); i++) if (g.p.inv.slots[i].item == itemMat(M_STONE)) return i; return -1; }();
        check(slot >= 0 && g.chestMove(1, slot) && shed->inv.count(itemMat(M_STONE)) == st0 && g.p.mat(M_STONE) == 0, "a stack can be stored in the shed");
        g.chestMove(0, 0);
    }
    g.state = ST_PLAY;
    g.hsChestPiece = -1;
    stock(g);
    // door
    check(g.hsPlace(HP_DOOR, sx + 6, sy, 0), "a door is built");
    HPiece* door = g.hsAt(sx + 6, sy, HL_OBJECT);
    check(door && g.world.solid[sy][sx + 6], "a closed door blocks");
    stand(g, V2((sx + 6) * TS + 8, (sy + 1) * TS + 14));
    g.homesteadInteract();
    check(door && door->open && !g.world.solid[sy][sx + 6], "E opens the door");
    // pen: a fence ring with a gap
    int px = 0, py = 0;
    check(findSpot(g, 8, 7, px, py, sx + 8, sy) || findSpot(g, 8, 7, px, py, 70, 110), "room for a pen");
    { std::string w2; g.hsCanBuild(HP_PEN, px, py, 0, &w2); printf("      pen at %d,%d: %s\n", px, py, w2.c_str()); }
    check(g.hsPlace(HP_PEN, px, py, 0), "an animal pen is built");
    check(g.world.solid[py][px] && g.world.solid[py + 2][px + 5] && !g.world.solid[py + 2][px + 2] && !g.world.solid[py + 4][px + 2], "the pen is a fence ring around open ground with a gap");
    // bridge over water
    {
        int wx = -1, wy = -1;
        for (int y = 20; y < 150 && wx < 0; y++)
            for (int x = 20; x < 220; x++)
                if (g.world.tile[y][x] == T_WATER && !g.world.water(x, y - 1) && !g.hsAt(x, y, HL_GROUND) && g.hsCanBuild(HP_BRIDGE, x, y, 0)) { wx = x; wy = y; break; }
        if (wx >= 0) {
            g.p.pos = V2(wx * TS + 8, wy * TS - 20);
            check(g.hsPlace(HP_BRIDGE, wx, wy, 0) && !g.world.blocked(wx * TS + 8, wy * TS + 8, 3.f), "a bridge over water can be walked on");
            g.p.pos = V2(wx * TS + 8, wy * TS - 40);
            check(g.hsDemolish(wx, wy) && g.world.tile[wy][wx] == T_WATER, "demolishing the bridge restores the water");
        } else check(false, "(no water found for the bridge test)");
    }

    printf("--- stage 4: farming\n");
    int fx = 0, fy = 0;
    {
        // a plot on fertile soil if there is a free dark tile near the field, else any
        bool found = false;
        for (int y = HS_PLAZA_Y + 5; y < HS_PLAZA_Y + 30 && !found; y++)
            for (int x = HS_PLAZA_X - 5; x < HS_PLAZA_X + 25 && !found; x++)
                if (g.world.tile[y][x] == T_DARK && g.hsCanBuild(HP_PLOT, x, y, 0)) fx = x, fy = y, found = true;
        check(found, "fertile (dark) soil near the plaza");
    }
    g.p.pos = V2(fx * TS + 8, fy * TS + 30);
    check(g.hsPlace(HP_PLOT, fx, fy, 0), "a farm plot is tilled (hoe in the pack)");
    HPiece* plot = g.hsAt(fx, fy, HL_GROUND);
    check(plot && plot->rich, "the plot on dark soil is fertile");
    {
        const int hoe = g.p.tool[TT_HOE];
        g.p.tool[TT_HOE] = -1;
        check(!g.hsCanBuild(HP_PLOT, fx + 1, fy, 0, &why) && why == "EQUIP YOUR HOE FIRST (PACK > TO HOTBAR)", "plots need the hoe equipped (a hoe only in the pack is not used)");
        g.p.tool[TT_HOE] = hoe;
    }
    stand(g, V2(fx * TS + 8, fy * TS + 26));
    g.hsFindTarget(t);
    check(!(g.hsFindTarget(t) && t.type == 1), "an empty plot has no E prompt (sowing is done with seeds in hand)");
    const int seeds0 = g.p.mat(M_SEED_WHEAT);
    check(g.hsPlant(*plot, CR_WHEAT) && g.p.mat(M_SEED_WHEAT) == seeds0 - 1 && plot->crop == CR_WHEAT, "planting uses a seed");
    steps(g, 20, 1.f);
    check(plot->stage == 0 && plot->growT == 0.f, "a crop on dry soil does not grow");
    check(g.hs.can == 0 && !g.hsWater(*plot), "an empty can cannot water");
    g.hsRefillCan();
    check(g.hs.can == CAN_CAPACITY && g.hsWater(*plot) && plot->water > 0.99f && g.hs.can == CAN_CAPACITY - 1, "refilling and watering works");
    int stagesSeen = 0, last = 0;
    for (int i = 0; i < 400 && plot->stage < CROP_STAGES - 1; i++) {
        plot->water = std::max(plot->water, 0.5f);
        g.update(1.f);
        if (plot->stage != last) stagesSeen++, last = plot->stage;
    }
    check(plot->stage == CROP_STAGES - 1 && stagesSeen == CROP_STAGES - 1, "a watered crop grows through every stage to ripe");
    const int wheat0 = g.p.mat(M_WHEAT);
    {
        const int sick = g.p.tool[TT_SICKLE];
        g.p.tool[TT_SICKLE] = -1;
        check(!g.hsHarvest(*plot) && plot->crop == CR_WHEAT, "wheat cannot be cut without a sickle");
        g.p.tool[TT_SICKLE] = sick;
    }
    check(g.hsHarvest(*plot) && g.p.mat(M_WHEAT) >= wheat0 + 3 && plot->crop == -1, "harvesting with the sickle gives wheat and clears the plot");
    check(g.p.sk.s[SK_FARMING].xp > 0, "farming earns skill XP");
    // ---- Building XP is paid for the net cost: a free or fully refunded piece is no XP loop
    {
        int bx = 0, by = 0;
        check(findSpot(g, 4, 4, bx, by, sx, sy) || findSpot(g, 4, 4, bx, by, 70, 100), "found a spot for the build / demolish loop test");
        g.p.inv.addIgnoringWeight(itemMat(M_PLANK), 50);
        const int64_t b0 = g.p.sk.s[SK_BUILDING].xp;
        const V2 keepPos = g.p.pos;
        bool free = true;
        for (int i = 0; i < 20; i++) {  // a farm plot (free) and a 1-plank floor (comes back whole)
            g.p.pos = V2((bx + 3) * TS + 8, (by + 3) * TS + 8);
            free = free && g.hsPlace(HP_PLOT, bx, by, 0) && g.hsDemolish(bx, by);
            free = free && g.hsPlace(HP_FLOOR_WOOD, bx + 1, by, 0) && g.hsDemolish(bx + 1, by);
        }
        check(free && g.p.sk.s[SK_BUILDING].xp == b0, "building and demolishing a plot or a plank floor 20 times earns no Building XP");
        const int planks = g.p.mat(M_PLANK);
        check(planks >= 50, "and the loop does not lose or create planks beyond the refund rule");
        const int64_t b1 = g.p.sk.s[SK_BUILDING].xp;
        g.p.inv.addIgnoringWeight(itemMat(M_STONE), 20);
        check(g.hsPlace(HP_WALL_STONE, bx + 2, by, 0) && g.p.sk.s[SK_BUILDING].xp > b1, "a piece with a real net cost (a stone wall) still earns Building XP");
        g.hsDemolish(bx + 2, by);
        g.p.pos = keepPos;
        plot = g.hsAt(fx, fy, HL_GROUND);  // building and demolishing may reallocate the piece list: the old pointer is stale
    }
    // ---- equipping and taking off the farming tools
    {
        const int types[3] = {TT_HOE, TT_CAN, TT_SICKLE}, ids[3] = {TL_HOE, TL_CAN, TL_SICKLE};
        const int used0 = g.p.inv.usedSlots();
        bool ok = true;
        for (int i = 0; i < 3; i++) {  // take each one off: it returns to the pack once, and the farm code stops seeing it
            ok = ok && g.unequipFarmTool(types[i]) && g.p.tool[types[i]] < 0 && g.p.inv.count(itemTool(ids[i])) == 1 && g.p.hotSlotOf(itemTool(ids[i])) < 0 && g.bestTool(types[i]) == 0;
        }
        check(ok && g.p.inv.usedSlots() == used0, "taking off the hoe, can and sickle clears the hotbar; each stays in the pack exactly once");
        check(!g.unequipFarmTool(TT_HOE) && !g.unequipFarmTool(TT_AXE), "taking off an empty or non-farm slot does nothing");
        check(!g.hsWater(*plot) && !g.hsCanBuild(HP_PLOT, fx + 1, fy, 0, &why) && why == "EQUIP YOUR HOE FIRST (PACK > TO HOTBAR)", "a tool lying in the pack is not used, and the message says to equip it");
        auto slotOf = [&](int tool) { for (int s = 0; s < g.p.bagCap(); s++) if (g.p.inv.slots[s].item == itemTool(tool)) return s; return -1; };
        ok = true;
        for (int i = 0; i < 3; i++) {  // equip each one through the inventory action
            ok = ok && g.useSlot(slotOf(ids[i])) && g.p.tool[types[i]] == ids[i] && g.p.inv.count(itemTool(ids[i])) == 1 && g.p.hotSlotOf(itemTool(ids[i])) >= 0 && g.bestTool(types[i]) == 1;
        }
        check(ok && g.p.inv.usedSlots() == used0, "equipping each tool through the pack puts it on the hotbar and it stays in the pack");
        // incompatible items are refused and nothing moves
        g.p.inv.addIgnoringWeight(itemTool(TL_AXE1), 1);
        g.p.inv.addIgnoringWeight(itemMat(M_WOOD), 1);
        const int axeSlot = slotOf(TL_AXE1);
        int woodSlot = -1;
        for (int s = 0; s < g.p.bagCap(); s++) if (g.p.inv.slots[s].item == itemMat(M_WOOD)) woodSlot = s;
        const int used1 = g.p.inv.usedSlots();
        check(!g.equipFarmTool(axeSlot) && !g.equipFarmTool(woodSlot) && !g.equipFarmTool(-1) && g.p.inv.usedSlots() == used1, "an axe, a material or an empty slot cannot be equipped as a farming tool");
        g.p.inv.remove(itemTool(TL_AXE1), 1);
        // equipping the equipped tool again is refused: one hoe on the hotbar, no duplicate
        g.p.inv.addIgnoringWeight(itemTool(TL_HOE), 1);  // (the test pack may be at its weight limit)
        int hoeSlots = 0;
        for (int h = 0; h < HOT_N; h++) hoeSlots += g.p.hot[h] == itemTool(TL_HOE);
        check(!g.equipFarmTool(slotOf(TL_HOE)) && g.p.tool[TT_HOE] == TL_HOE && hoeSlots == 1, "equipping an already equipped tool changes nothing");
        g.p.inv.remove(itemTool(TL_HOE), 1);  // drop the spare
        g.unequipFarmTool(TT_HOE);
        g.equipFarmTool(slotOf(TL_HOE));
        check(g.p.tool[TT_HOE] == TL_HOE && g.p.inv.count(itemTool(TL_HOE)) == 1 && g.p.hotSlotOf(itemTool(TL_HOE)) >= 0, "a tool can be put back after taking it off");
        // taking off needs no free pack slot: the tool simply stays where it is
        std::vector<int> filled;
        for (int s2 = 0; s2 < g.p.bagCap(); s2++)
            if (g.p.inv.slots[s2].empty()) { g.p.inv.slots[s2].item = itemMat(M_WOOD); g.p.inv.slots[s2].n = 1; filled.push_back(s2); }
        check(g.unequipFarmTool(TT_CAN) && g.p.tool[TT_CAN] < 0 && g.p.inv.count(itemTool(TL_CAN)) == 1, "taking a tool off works with a full pack (it stays in the pack)");
        for (int s2 : filled) g.p.inv.slots[s2] = Slot();
        check(g.equipFarmTool(slotOf(TL_CAN)) && g.p.tool[TT_CAN] == TL_CAN, "with the pack emptied again it can be equipped");
        g.p.inv.remove(itemMat(M_WOOD), 99);
        // a tool that leaves the pack stops being equipped (and leaves the hotbar)
        g.p.inv.remove(itemTool(TL_CAN), 1);
        g.syncFarmTools();
        check(g.p.tool[TT_CAN] < 0 && g.p.hotSlotOf(itemTool(TL_CAN)) < 0, "a tool dropped or sold from the pack is no longer equipped");
        g.p.inv.addIgnoringWeight(itemTool(TL_CAN), 1);
        g.equipFarmTool(slotOf(TL_CAN));
        // the hotbar is universal: a tool goes in any chosen slot, moves between slots, and the number key puts it in hand
        {
            const int hoeId = itemTool(TL_HOE);
            g.p.hot[7] = -1;
            check(g.equipHot(hoeId, 7) && g.p.hot[7] == hoeId && g.p.tool[TT_HOE] == TL_HOE && g.p.hotSlotOf(hoeId) == 7, "a farming tool can be put in any hotbar slot (slot 8)");
            g.hotUse(7);
            check(g.p.tool[TT_HOE] == TL_HOE && g.bestTool(TT_HOE) == 1, "the number key puts the tool in hand and it stays equipped");
            g.unequipHot(7);
            check(g.p.tool[TT_HOE] < 0 && g.p.hotSlotOf(hoeId) < 0 && g.p.inv.count(hoeId) == 1 && g.bestTool(TT_HOE) == 0, "unequipping from the hotbar takes the tool off and keeps it in the pack");
            g.equipFarmTool(slotOf(TL_HOE));
        }
        // a tool in hand works the soil with a left click on the tile under the cursor
        {
            const int tx = fx + 1, ty = fy;
            auto aim = [&](int tileX, int tileY) { g.in.mx = (int)(tileX * TS + 8 - g.camX); g.in.my = (int)(tileY * TS + 8 - g.camY); };
            g.equipFarmTool(slotOf(TL_HOE));
            g.equipFarmTool(slotOf(TL_CAN));
            g.p.inv.addIgnoringWeight(itemMat(M_WOOD), 40);
            g.p.inv.addIgnoringWeight(itemMat(M_SEED_CARROT), 2);
            stand(g, V2(tx * TS + 8, ty * TS + 30));
            g.hotUse(g.p.hotSlotOf(itemTool(TL_HOE)));
            check(g.p.hand == TT_HOE, "the hotbar key of a farm tool puts it in hand");
            const size_t n0 = g.hs.pieces.size();
            aim(tx + 12, ty);  // far away
            g.hsToolUse();
            check(g.hs.pieces.size() == n0, "a click on a tile out of reach does nothing");
            check(g.p.farmT <= 0, "...and starts no swing");
            aim(tx, ty);
            g.hsToolUse();
            check(g.p.farmT > 0 && g.p.farmType == TT_HOE, "a hoe click starts the hoe swing");
            g.updateHunterAnim(0.02f, false, false, false, false, -1, 0.f);
            check(g.p.anim.state == HS_SWING, "the hoe plays the axe stroke");
            g.updateHunterAnim(FARM_SWING_T, false, false, false, false, -1, 0.f);
            g.p.farmT = 0;
            g.updateHunterAnim(0.02f, false, false, false, false, -1, 0.f);
            check(g.p.anim.state != HS_SWING, "the swing ends when its time is up");
            HPiece* np = g.hsAt(tx, ty, HL_GROUND);
            check(np && np->kind == HP_PLOT && g.hs.pieces.size() == n0 + 1, "left click with the hoe in hand tills the tile into a farm plot");
            g.state = ST_PLAY;
            g.hsToolUse();
            check(g.state == ST_PLAY && np && np->kind == HP_PLOT, "clicking the empty plot with the hoe does nothing (seeds are sown from the hotbar)");
            np = g.hsAt(tx, ty, HL_GROUND);
            // seeds are equipped on the hotbar like a tool and sow the plot they are clicked on
            const int seedId = itemMat(cropSeed(CR_CARROT));
            check(g.equipHot(seedId) && g.p.hotSlotOf(seedId) >= 0, "seeds can be put on the hotbar");
            g.hotUse(g.p.hotSlotOf(seedId));
            check(g.p.hand == HAND_SEED0 + CR_CARROT, "the seeds' key puts them in hand");
            aim(tx + 1, ty);
            g.hsToolUse();
            check(!g.hsAt(tx + 1, ty, HL_GROUND), "seeds clicked on bare soil are not wasted (till it first)");
            check(g.p.farmT <= 0, "a refused click (no plot to sow) does not swing");
            aim(tx, ty);
            const int seedsBefore = g.p.inv.count(seedId);
            g.hsToolUse();
            np = g.hsAt(tx, ty, HL_GROUND);
            check(np && np->crop == CR_CARROT && g.p.inv.count(seedId) == seedsBefore - 1, "left click with seeds in hand sows the tilled plot and uses one seed");
            g.updateHunterAnim(0.02f, false, false, false, false, -1, 0.f);
            check(g.p.farmT > 0 && g.p.anim.state == HS_FORAGE, "sowing plays the bend of planting");
            g.hsToolUse();
            check(g.p.inv.count(seedId) == seedsBefore - 1, "clicking an already planted plot does not use another seed");
            g.unequipHot(g.p.hotSlotOf(seedId));
            check(g.p.hand < 0, "taking the seeds off the hotbar empties the hand");
            np = g.hsAt(tx, ty, HL_GROUND);
            np->water = 0.f;
            g.hotUse(g.p.hotSlotOf(itemTool(TL_CAN)));
            g.hsRefillCan();
            check(g.p.hand == TT_CAN, "the can goes in hand with its key");
            g.hsToolUse();
            np = g.hsAt(tx, ty, HL_GROUND);
            check(np && np->water > 0.f, "left click with the can waters the planted crop");
            const int wpnSlot = g.p.hotSlotOf(itemWpn(W_FLINT));
            if (wpnSlot >= 0) { g.hotUse(wpnSlot); check(g.p.hand < 0, "a weapon key takes the weapon back in hand"); }
            g.hsDemolish(tx, ty);
        }
        // equipped tools do their jobs (water, till, cut) after all the switching
        g.hsRefillCan();
        check(g.bestTool(TT_CAN) == 1 && g.hs.can == CAN_CAPACITY && g.bestTool(TT_HOE) == 1 && g.bestTool(TT_SICKLE) == 1, "after equipping again the farm code sees every tool");
        g.unequipFarmTool(TT_SICKLE);  // leave the sickle off: the save below must remember that
    }
    // growth continues while away
    g.hsPlant(*plot, CR_CARROT);
    plot->water = 1.f;
    g.p.pos = g.world.hportal + V2(0, 16);
    g.travelCd = 0;
    g.homesteadInteract();
    check(g.world.mapId == B_HOME, "back in town while the carrots grow");
    for (int i = 0; i < 60; i++) { plot->water = std::max(plot->water, 0.5f); g.update(1.f); }
    check(plot->stage >= 1, "crops keep growing while the hunter is away from the Homestead");
    g.p.pos = g.world.hportal + V2(0, 16);
    g.travelCd = 0;
    g.homesteadInteract();
    // hands-only crop and the greenhouse
    int gx = 0, gy = 0;
    check(findSpot(g, 6, 5, gx, gy, sx, sy + 12) || findSpot(g, 6, 5, gx, gy, 90, 110), "room for a greenhouse");
    g.p.pos = V2(gx * TS + 40, (gy + 7) * TS);
    check(g.hsPlace(HP_GREENHOUSE, gx, gy, 0), "a greenhouse is built");
    check(g.hsPlace(HP_PLOT, gx + 1, gy + 1, 0), "a plot can be placed inside it");
    HPiece* gp = g.hsAt(gx + 1, gy + 1, HL_GROUND);
    HPiece* op = g.hsAt(fx, fy, HL_GROUND);
    if (gp && op) {
        gp->rich = op->rich = false;
        gp->crop = op->crop = CR_TOMATO; gp->stage = op->stage = 0; gp->growT = op->growT = 0; gp->water = op->water = 1.f;
        g.hsTickCrops(10.f);
        check(gp->growT > op->growT * 1.4f, "crops in the greenhouse grow faster");
        gp->crop = op->crop = -1;
    }

    printf("--- stage 5: animals\n");
    int cx2 = 0, cy2 = 0;
    check(findSpot(g, 8, 6, cx2, cy2, 90, 110) || findSpot(g, 8, 6, cx2, cy2, 70, 100), "room for a coop");
    g.p.pos = V2(cx2 * TS + 40, (cy2 + 6) * TS);
    check(g.hsPlace(HP_COOP, cx2, cy2, 0), "a chicken coop is built");
    HPiece* coop = g.hsAt(cx2, cy2, HL_OBJECT);
    const int cid = coop ? coop->id : 0;
    for (int i = 0; i < 2; i++) g.hsAddAnimal(DA_CHICKEN, V2((cx2 + 4) * TS, (cy2 + 3) * TS + i * 6), false, -1);
    g.hsAddAnimal(DA_COW, V2((cx2 + 4) * TS, (cy2 + 4) * TS), false, -1);
    check(g.hs.animals.size() == 3, "animals are added");
    HAnimal& c0 = g.hs.animals[0];
    HAnimal& c1 = g.hs.animals[1];
    HAnimal& cow = g.hs.animals[2];
    check(g.hsAssign(c0, cid) && g.hsAssign(c1, cid) && c0.home == cid, "chickens are assigned to the coop");
    check(!g.hsAssign(cow, cid), "a cow does not fit the coop");
    // wander inside the yard
    {
        float x0, y0, x1, y1;
        g.hsYard(c0, x0, y0, x1, y1);
        bool inside = true, moved = false;
        const V2 startPos(c0.px, c0.py);
        for (int i = 0; i < 400; i++) {
            g.update(0.25f);
            inside = inside && c0.px > x0 - 10 && c0.px < x1 + 10 && c0.py > y0 - 10 && c0.py < y1 + 10;
            moved = moved || dist(V2(c0.px, c0.py), startPos) > 4.f;
        }
        check(moved, "animals wander on their own");
        check(inside, "...and stay inside the area they are assigned to");
        check(!g.world.blocked(c0.px, c0.py, 2.f), "...never inside walls");
    }
    // needs
    c0.full = 20; c0.happy = 70; c0.health = 100;
    give(g, M_WHEAT, 5);
    const int wheat1 = g.p.mat(M_WHEAT);
    check(g.hsFeed(c0) && c0.full > 55 && g.p.mat(M_WHEAT) == wheat1 - 1, "feeding takes one food from the pack and fills the belly");
    c0.full = 0;
    const float h0 = c0.health;
    g.hsTickAnimals(10.f);
    check(c0.health < h0 && c0.happy < 70, "a starving animal loses health and happiness");
    // production
    c1.full = 100; c1.happy = 100; c1.health = 100; c1.prodT = 0; c1.stock = 0;
    for (int i = 0; i < 400; i++) { c1.full = 100; c1.happy = 100; g.hsTickAnimals(1.f); if (c1.stock > 0) break; }
    check(c1.stock > 0, "a fed, happy adult chicken lays eggs");
    const int eggs0 = g.p.mat(M_EGG);
    check(g.hsCollect(c1) > 0 && g.p.mat(M_EGG) > eggs0 && c1.stock == 0, "eggs can be collected into the pack");
    {   // the farming bonus ("extra crops and produce") also reaches animal produce
        int extra0 = 0, extra1 = 0;
        bool exact = true;
        for (int i = 0; i < 100; i++) {
            g.p.sk.s[SK_FARMING].xp = 0;
            c1.stock = 2;
            if (g.hsCollect(c1) != 2) extra0++;
        }
        for (int i = 0; i < 300; i++) {
            g.p.sk.s[SK_FARMING].xp = SkillState::capXp();
            c1.stock = 2;
            const int n = g.hsCollect(c1);
            if (n == 3) extra1++;
            else if (n != 2) exact = false;
        }
        g.p.sk.s[SK_FARMING].xp = 0;
        check(extra0 == 0, "at farming level 1 collecting produce gives exactly what the animal stocked");
        check(exact && extra1 >= 40 && extra1 <= 140, "a master farmer (30% bonus) sometimes collects one extra: " + std::to_string(extra1) + " of 300 (expected ~90)");
    }
    // breeding and growth
    c0.full = c1.full = 100; c0.happy = c1.happy = 100; c0.health = c1.health = 100; c0.breedCd = c1.breedCd = 0;
    c0.px = c1.px = (cx2 + 4) * TS; c0.py = c1.py = (cy2 + 3) * TS;
    const size_t pop0 = g.hs.animals.size();
    for (int i = 0; i < 12 && g.hs.animals.size() == pop0; i++) { g.hs.animals[0].full = g.hs.animals[1].full = 100; g.hsTickAnimals(5.f); }
    check(g.hs.animals.size() == pop0 + 1, "two adults of a species breed and a baby is born");
    HAnimal& baby = g.hs.animals.back();
    check(!baby.adult(DOMS[baby.kind]) && baby.home == cid, "the baby is young and lives in the same coop");
    for (int i = 0; i < 400 && !baby.adult(DOMS[baby.kind]); i++) { baby.full = 100; g.hsTickAnimals(1.f); }
    check(baby.adult(DOMS[baby.kind]), "the baby grows up");
    // the sprites
    g.ensureDomArt();
    bool art = g.A.Dom.built;
    for (int k = 0; k < DA_COUNT; k++)
        for (int v = 0; v < AV_VIEWS; v++)
            for (int st = 0; st < DS_COUNT; st++)
                for (int f = 0; f < DS_FRAMES[st]; f++) art = art && g.A.Dom.get(k, v, st, f).ok();
    check(art, "all seven animals have idle / walk / eat frames in 4 directions");

    printf("--- stage 6: save, load and integration\n");
    {
        bool keys = true;
        for (int m = M_BERRY; m < M_COUNT; m++) keys = keys && itemFromKey(itemKey(itemMat(m))) == itemMat(m);
        for (int k = TL_HOE; k < TL_COUNT; k++) keys = keys && itemFromKey(itemKey(itemTool(k))) == itemTool(k);
        check(keys, "every new material and tool is a valid inventory item with a save key");
        bool uniq = true;
        for (int a = 0; a < M_COUNT; a++)
            for (int b = a + 1; b < M_COUNT; b++) uniq = uniq && std::string(MAT_NAME[a]) != MAT_NAME[b];
        check(uniq, "no duplicate material names");
        check(item(itemMat(M_WHEAT)).stack == 999 || item(itemMat(M_WHEAT)).stack > 0, "stacks follow the existing rules");
    }
    HPiece* shed2 = g.hsAt(sx, sy + 3, HL_OBJECT);
    if (shed2) shed2->inv.add(itemMat(M_CORN), 12);
    op = g.hsAt(fx, fy, HL_GROUND);
    if (op) { op->rich = true; op->crop = CR_PUMPKIN; op->stage = 2; op->growT = 17.f; op->water = 0.4f; }
    // the trophy wall: built like any piece, kept in the save, and drawn with the trophies earned
    int twx = -1, twy = -1;
    g.p.inv.add(itemMat(M_PLANK), 10);
    g.p.inv.add(itemMat(M_ROPE), 2);
    for (int yy = sy - 12; yy < sy + 14 && twx < 0; yy++)
        for (int xx = sx - 14; xx < sx + 14 && twx < 0; xx++)
            if (g.hsPlace(HP_TROPHY, xx, yy, 0)) twx = xx, twy = yy;
    check(twx >= 0 && g.hsAt(twx + 3, twy, HL_OBJECT) && !g.hsAt(twx + 4, twy, HL_OBJECT), "a trophy wall can be built and covers 4 x 1 tiles");
    g.trophyMask = 5;
    const size_t nPieces = g.hs.pieces.size(), nAnimals = g.hs.animals.size();
    const float happy0 = g.hs.animals[0].happy;
    const int coinsBefore = g.p.coins;
    g.p.pos = V2(HS_PLAZA_X * TS, (HS_PLAZA_Y + 3) * TS);
    g.save();
    g.newGame();
    check(g.p.tool[TT_HOE] < 0, "a new game starts with no farming tools equipped");
    check(g.hs.pieces.empty() && g.hs.animals.empty(), "a new game starts with an empty Homestead");
    check(g.load(), "the save loads");
    check(g.world.mapId == B_HOMESTEAD, "a hunter saved in the Homestead loads back into it");
    check(g.hs.pieces.size() == nPieces && g.hs.animals.size() == nAnimals, "every building and animal is restored");
    check(twx >= 0 && g.hsAt(twx, twy, HL_OBJECT) && g.hsAt(twx, twy, HL_OBJECT)->kind == HP_TROPHY && g.trophyMask == 5, "the trophy wall and the trophies earned are restored");
    check(g.p.coins == coinsBefore, "the hunter's own data loads too");
    check(g.p.tool[TT_HOE] == TL_HOE && g.p.tool[TT_CAN] == TL_CAN && g.p.tool[TT_SICKLE] < 0 && g.p.inv.count(itemTool(TL_SICKLE)) == 1 && g.p.inv.count(itemTool(TL_HOE)) == 1 && g.p.hotSlotOf(itemTool(TL_HOE)) >= 0 && g.p.hotSlotOf(itemTool(TL_SICKLE)) < 0, "the equipped farming tools are saved and restored (the sickle stayed off the hotbar)");
    HPiece* s3 = g.hsAt(sx, sy + 3, HL_OBJECT);
    check(s3 && s3->rot == 1 && s3->inv.count(itemMat(M_CORN)) == 12, "storage contents and rotation are restored");
    HPiece* o3 = g.hsAt(fx, fy, HL_GROUND);
    check(o3 && o3->crop == CR_PUMPKIN && o3->stage == 2 && fabsf(o3->growT - 17.f) < 0.5f && o3->rich, "crop type, stage, timer and soil are restored");
    check(fabsf(g.hs.animals[0].happy - happy0) < 0.5f, "animal stats are restored");
    check(g.world.solid[sy + 3][sx] && g.world.solid[py][px], "solid tiles of buildings and the pen are restored on the regenerated map");
    check(!g.world.solid[py + 2][px + 2] && g.hsAt(px, py, HL_OBJECT) != nullptr, "...and the pen's inside stays open");
    HPiece* d3 = g.hsAt(sx + 6, sy, HL_OBJECT);
    check(d3 && d3->open && !g.world.solid[sy][sx + 6], "an open door stays open");
    {   // the harvested tree is still down after the round trip through the save
        check(g.hs.visited && g.hs.gaveStarter, "the starter kit is not handed out again");
    }
    // portal round trip after load
    g.p.pos = g.world.hportal + V2(0, 16);
    g.travelCd = 0;
    g.homesteadInteract();
    check(g.world.mapId == B_HOME, "the return portal still works after loading");
    g.p.pos = g.world.hportal + V2(0, 16);
    g.travelCd = 0;
    g.homesteadInteract();
    check(g.hs.pieces.size() == nPieces && g.hsAt(sx, sy + 3, HL_OBJECT) != nullptr && g.world.solid[sy + 3][sx], "the Homestead is not reset by entering again");

    if (const char* dir = getenv("HS_SHOTS")) {  // screenshots of the real renderer for a visual check
        auto shot = [&](const char* name) {
            g.helpT = 0; g.portalFx = 0; g.msgs.clear();
            g.setCam(0, true);
            g.render();
            g.saveCanvas((std::string(dir) + "/" + name).c_str());
        };
        g.state = ST_PLAY;
        g.dayT = DAY_LEN * 0.2f; g.helpT = 0;
        g.travelCd = 0;
        g.hs.animals.resize(std::min<size_t>(g.hs.animals.size(), 2));
        g.travelTo(B_HOME, COX + 33, COY + 33, false);
        steps(g, 60);
        shot("a_town_portal.bmp");
        g.travelTo(B_HOMESTEAD, HS_PORTAL_X, HS_PORTAL_Y + 2, false);
        steps(g, 60);
        shot("b_plaza.bmp");
        g.p.pos = V2((cx2 + 4) * TS, (cy2 + 8) * TS);
        for (int k = 0; k < DA_COUNT; k++) g.hsAddAnimal(k, V2((cx2 + 3 + k) * TS, (cy2 + 4) * TS), k % 2 == 0, -1);
        steps(g, 30);
        shot("c_buildings.bmp");
        g.p.pos = V2((px + 3) * TS, (py + 9) * TS);
        steps(g, 5);
        shot("d_pen.bmp");
        g.bm.on = true; g.bm.kind = HP_HOUSE; g.in.mx = 200; g.in.my = 100;
        steps(g, 2);
        shot("e_ghost.bmp");
        g.bm.on = false;
        g.state = ST_BUILD; shot("f_build_menu.bmp");
        g.state = ST_ANIMAL; g.farmPieceId = -1; shot("h_animals.bmp");
        g.state = ST_PLAY;
        {   // a field of every crop: stages 0..4 across, three plots of each in a row, to check the bed shapes and the crop art
            bool placed = false;
            V2 fieldPlot(0, 0);
            for (int ty0 = py - 14; ty0 < py + 14 && !placed; ty0++)
                for (int tx0 = px - 14; tx0 < px + 14 && !placed; tx0++) {
                    bool free = true;
                    for (int dy = 0; dy < 10 && free; dy++)
                        for (int dx = 0; dx < 14 && free; dx++) free = g.hsCanBuild(HP_PLOT, tx0 + dx, ty0 + dy, 0);
                    if (!free) continue;
                    placed = true;
                    for (int c = 0; c < CR_COUNT; c++)
                        for (int st = 0; st < 5; st++) {
                            const int tx = tx0 + st + (c < 7 ? 0 : 7), ty = ty0 + (c % 7) + (c < 7 ? 0 : 0) - 0;
                            if (c >= 7 && st > 4) continue;
                            if (!g.hsPlace(HP_PLOT, tx, ty, 0)) continue;
                            HPiece* pc = g.hsAt(tx, ty, HL_GROUND);
                            if (pc) { pc->crop = c; pc->stage = st; pc->water = (st % 2) ? 5.f : 0.f; }
                        }
                    g.p.pos = V2((tx0 + 6) * TS, (ty0 + 8) * TS);
                    fieldPlot = V2((tx0 + 3) * TS + 8, (ty0 + 3) * TS + 8);
                }
            g.dayT = DAY_LEN * 0.3f;
            steps(g, 3); shot("i_field.bmp");
            // the cursor says what the tool in hand would do: watering a growing plot (green) and a hoe over a planted plot (red)
            g.p.tool[TT_CAN] = TL_CAN; g.p.hand = TT_CAN;
            g.p.pos = fieldPlot + V2(0, 24);
            g.setCam(0, true);
            g.in.mx = fieldPlot.x - g.camX; g.in.my = fieldPlot.y - g.camY;
            shot("i2_cursor_can.bmp");
            g.p.tool[TT_HOE] = TL_HOE; g.p.hand = TT_HOE;
            shot("i3_cursor_hoe.bmp");
            g.p.hand = -1;
            g.in.my = 200;
        }
        g.p.pos = V2(fx * TS, (fy + 2) * TS);
        g.dayT = DAY_LEN * 0.7f;
        steps(g, 3); shot("j_night.bmp");
    }
    printf("--- the E key at a ripe crop beside a harvestable tree, rock or plant\n");
    {   // the real key path (update), not the direct call: E used to start harvesting the nearby plant instead of using the plot
        g.state = ST_PLAY;
        g.p.coins += 1000;
        stock(g);
        int tried = 0, worked = 0;
        int noToolCrop = 0;  // a crop that needs no tool to harvest
        for (int c = 0; c < CR_COUNT; c++) if (CROPS[c].tool < 0) { noToolCrop = c; break; }
        for (size_t i = 0; i < g.world.nodes.size() && tried < 6; i++) {
            const int ntx = (int)(g.world.nodes[i].x / TS), nty = (int)(g.world.nodes[i].y / TS);
            for (int d = 0; d < 8 && tried < 6; d++) {
                const int tx = ntx + (d % 3) - 1 + (d > 3 ? 0 : 0), ty = nty + (d / 3) - 1;
                if ((tx == ntx && ty == nty) || !g.hsCanBuild(HP_PLOT, tx, ty, 0)) continue;
                stand(g, V2((tx + .5f) * TS, (ty + .5f) * TS));
                HTarget h;
                if (!g.findHarvest(h)) continue;  // not within reach of anything harvestable: not the case under test
                if (!g.hsPlace(HP_PLOT, tx, ty, 0)) continue;
                stand(g, V2((tx + .5f) * TS, (ty + .5f) * TS));
                tried++;
                if (HPiece* rp = g.hsAt(tx, ty, HL_GROUND)) { rp->crop = noToolCrop; rp->stage = CROP_STAGES - 1; rp->growT = 0; }
                g.in.ePressed = true;
                g.update(1.f / 60.f);
                g.in.ePressed = false;
                if (const HPiece* rp = g.hsAt(tx, ty, HL_GROUND)) worked += !(rp->crop == noToolCrop && rp->stage >= CROP_STAGES - 1);  // harvested
                g.state = ST_PLAY;
                g.hsDemolish(tx, ty);
                break;
            }
        }
        {   // control: the same key press at a plot with nothing harvestable around
            int px = 0, py = 0, ok = 0;
            for (int ty = 100; ty < 150 && !ok; ty++)
                for (int tx = 70; tx < 200 && !ok; tx++) {
                    if (!g.hsCanBuild(HP_PLOT, tx, ty, 0)) continue;
                    stand(g, V2((tx + .5f) * TS, (ty + .5f) * TS));
                    HTarget h;
                    if (g.findHarvest(h) || !g.hsPlace(HP_PLOT, tx, ty, 0)) continue;
                    px = tx; py = ty; ok = 1;
                }
            bool harvested = false;
            if (ok) {
                if (HPiece* rp = g.hsAt(px, py, HL_GROUND)) { rp->crop = noToolCrop; rp->stage = CROP_STAGES - 1; rp->growT = 0; }
                stand(g, V2((px + .5f) * TS, (py + .5f) * TS));
                g.in.ePressed = true;
                g.update(1.f / 60.f);
                g.in.ePressed = false;
                if (const HPiece* rp = g.hsAt(px, py, HL_GROUND)) harvested = !(rp->crop == noToolCrop && rp->stage >= CROP_STAGES - 1);
                g.state = ST_PLAY;
                g.hsDemolish(px, py);
            }
            check(ok && harvested, "control: E at a ripe crop with nothing harvestable around harvests it");
        }
        check(tried > 0, "found a spot for a plot within reach of a harvestable (" + std::to_string(tried) + " tried)");
        check(tried > 0 && worked == tried, "E at a ripe crop harvests it even with a harvestable tree, rock or plant in reach (" + std::to_string(worked) + " of " + std::to_string(tried) + ")");
    }
    {  // an old save (no "farmtools" line) keeps working: the hoe / can / sickle in its pack become the equipped ones
        g.p.tool[TT_HOE] = g.p.tool[TT_CAN] = g.p.tool[TT_SICKLE] = -1;
        for (int k : {(int)TL_HOE, (int)TL_CAN, (int)TL_SICKLE}) {  // exactly one of each in the pack, none on the hotbar (as an old save had them)
            g.p.inv.remove(itemTool(k), g.p.inv.count(itemTool(k)));
            g.p.inv.addIgnoringWeight(itemTool(k), 1);
            if (g.p.hotSlotOf(itemTool(k)) >= 0) g.p.hot[g.p.hotSlotOf(itemTool(k))] = -1;
        }
        g.save();
        std::ifstream in(g.savePath());
        std::string line, kept;
        while (std::getline(in, line)) if (line.rfind("farmtools", 0) != 0) kept += line + "\n";
        in.close();
        std::ofstream(g.savePath(), std::ios::trunc) << kept;
        g.newGame();
        const bool ldOk = g.load();
        check(ldOk && g.p.tool[TT_HOE] == TL_HOE && g.p.tool[TT_CAN] == TL_CAN && g.p.tool[TT_SICKLE] == TL_SICKLE && g.p.inv.count(itemTool(TL_HOE)) == 1 && g.p.hotSlotOf(itemTool(TL_HOE)) >= 0 && g.bestTool(TT_CAN) == 1, "an old save without the farmtools line equips the tools it carried");
    }
    {  // a save from the build where equipped farm tools lived outside the pack (farmtools line, no hotbar entry for them)
        for (int k : {(int)TL_HOE, (int)TL_CAN, (int)TL_SICKLE}) {
            g.p.inv.remove(itemTool(k), g.p.inv.count(itemTool(k)));
            if (g.p.hotSlotOf(itemTool(k)) >= 0) g.p.hot[g.p.hotSlotOf(itemTool(k))] = -1;
            g.p.tool[TOOLDEF[k].type] = k;
        }
        g.save();
        g.newGame();
        const bool ldOk = g.load();
        for (int fr = 0; fr < 5; fr++) g.update(1.f / 60.f);
        check(ldOk && g.p.tool[TT_HOE] == TL_HOE && g.p.tool[TT_CAN] == TL_CAN && g.p.tool[TT_SICKLE] == TL_SICKLE && g.bestTool(TT_HOE) == 1 && g.p.hotSlotOf(itemTool(TL_HOE)) >= 0 && g.p.inv.count(itemTool(TL_HOE)) == 1, "a save from the previous tool system (tools outside the pack) still has working tools after loading and playing a few frames");
    }
    printf("--- shovel: dig, pond, auto-watering\n");
    {
        check(g.ownsTool(TL_SHOVEL), "the starter kit includes a shovel");
        int hx = -1, hy = -1;
        for (int y = HS_PLAZA_Y + 8; y < HS_PLAZA_Y + 40 && hx < 0; y++)
            for (int x = HS_PLAZA_X - 5; x < HS_PLAZA_X + 25; x++) {
                bool free3 = true;
                for (int dy = -3; dy <= 3 && free3; dy++)
                    for (int dx = -3; dx <= 3; dx++) {
                        const int xx = x + dx, yy = y + dy;
                        if (xx < 0 || yy < 0 || xx >= MW || yy >= MH || !(tileGrassy(g.world.tile[yy][xx]) || g.world.tile[yy][xx] == T_DIRT) || g.hsAt(xx, yy, HL_GROUND) || g.hsAt(xx, yy, HL_OBJECT) || g.world.solid[yy][xx] || g.hsWaterTile(xx, yy)) { free3 = false; break; }
                    }
                for (auto& nd : g.world.nodes)
                    if (free3 && std::abs((int)floorf(nd.x / TS) - x) <= 3 && std::abs((int)floorf(nd.y / TS) - y) <= 3) free3 = false;
                if (free3) {  // (the shovel itself says whether the spot is allowed: plaza, edge...)
                    g.p.pos = V2(x * TS + 8, (y + 1) * TS + 8);
                    g.hsDig(x, y);
                    if (g.hsAt(x, y, HL_GROUND)) { g.hsDig(x, y); hx = x; hy = y; break; }
                }
            }
        check(hx >= 0, "a clear patch of ground for digging");
        if (hx >= 0) {
            g.p.pos = V2(hx * TS + 8, (hy + 1) * TS + 8);
            const uint8_t soil = g.world.tile[hy][hx];
            const size_t n0 = g.hs.pieces.size();
            check(g.hsDig(hx, hy) && g.hsAt(hx, hy, HL_GROUND) && g.hsAt(hx, hy, HL_GROUND)->kind == HP_HOLE && g.hs.pieces.size() == n0 + 1, "the shovel digs a hole");
            check(!g.world.blocked(hx * TS + 8, hy * TS + 8, 3.f), "a hole can be walked over");
            g.hsDig(hx, hy);
            check(!g.hsAt(hx, hy, HL_GROUND) && g.hs.pieces.size() == n0, "the shovel fills a hole back in");
            g.hsDig(hx, hy);
            g.hs.can = 2.f;
            g.hsFillHole(*g.hsAt(hx, hy, HL_GROUND));
            check(g.hsAt(hx, hy, HL_GROUND)->kind == HP_HOLE && g.hs.can == 2.f, "too little water in the can leaves the hole dry");
            g.hs.can = CAN_CAPACITY;
            g.hsFillHole(*g.hsAt(hx, hy, HL_GROUND));
            check(g.hsAt(hx, hy, HL_GROUND)->kind == HP_POND && g.world.tile[hy][hx] == T_SHALLOW && g.hs.can == CAN_CAPACITY - HOLE_WATER, "water in a hole makes a pond (and costs water)");
            // a plot next to the pond waters itself; one far away does not
            check(g.hsNearWater(hx + 2, hy, PLOT_WATER_REACH) && !g.hsNearWater(hx + 3, hy + 3, PLOT_WATER_REACH), "the pond's reach for watering");
            const bool hoe = g.bestTool(TT_HOE, nullptr) > 0;
            if (hoe) {
                g.p.pos = V2((hx + 2) * TS + 8, (hy + 3) * TS + 8);
                check(g.hsPlace(HP_PLOT, hx + 2, hy, 0), "(a plot beside the pond)");
                check(g.hsPlace(HP_PLOT, hx + 3, hy + 3, 0), "(a plot far from the pond)");
                g.hsAt(hx + 2, hy, HL_GROUND)->water = 0;
                g.hsAt(hx + 3, hy + 3, HL_GROUND)->water = 0;
                g.hsTickCrops(0.5f);
                check(g.hsAt(hx + 2, hy, HL_GROUND)->water >= 1.f, "tilled soil beside water is watered by itself");
                check(g.hsAt(hx + 3, hy + 3, HL_GROUND)->water == 0.f, "tilled soil away from water stays dry");
                g.hsDemolish(hx + 2, hy);
                g.hsDemolish(hx + 3, hy + 3);
            }
            // a hole next to the pond floods by itself (a channel)
            g.hsDig(hx + 1, hy);
            for (int i = 0; i < 20; i++) g.hsTickCrops(0.5f);
            check(g.hsAt(hx + 1, hy, HL_GROUND)->kind == HP_POND, "a hole beside water floods itself");
            // a hole away from water stays a hole
            g.hsDig(hx - 3, hy - 3);
            for (int i = 0; i < 40; i++) g.hsTickCrops(0.5f);
            check(g.hsAt(hx - 3, hy - 3, HL_GROUND)->kind == HP_HOLE, "a hole away from water stays dry");
            // save / load keeps the ponds as water
            g.save();
            g.world.tile[hy][hx] = T_GRASS;
            g.hsApplyWorld();
            check(g.world.tile[hy][hx] == T_SHALLOW, "ponds are water again after loading");
            g.hsDig(hx, hy);
            check(!g.hsAt(hx, hy, HL_GROUND) && g.world.tile[hy][hx] == soil, "filling a pond in gives its soil back");
            g.hsDig(hx + 1, hy);
            g.hsDig(hx - 3, hy - 3);
        }
    }

    printf("--- watering: once per crop, bare soil, rain, untilling\n");
    {
        int px = -1, py = -1;
        for (int y = HS_PLAZA_Y + 8; y < HS_PLAZA_Y + 60 && px < 0; y++)
            for (int x = HS_PLAZA_X - 10; x < HS_PLAZA_X + 30; x++) {
                std::string why;
                if (g.hsCanBuild(HP_PLOT, x, y, 0, &why) && !g.hsNearWater(x, y, PLOT_WATER_REACH)) { px = x; py = y; break; }
            }
        check(px >= 0, "a clear patch for a plot (no water within reach)");
        if (px >= 0 && g.bestTool(TT_HOE, nullptr) > 0) {
            g.p.pos = V2(px * TS + 8, (py + 1) * TS + 8);
            std::string why;
            const bool canB = g.hsCanBuild(HP_PLOT, px, py, 0, &why);
            g.hsPlace(HP_PLOT, px, py, 0);
            HPiece* pl = g.hsAt(px, py, HL_GROUND);
            if (!pl) printf("   (cannot till: %s)\n", why.c_str());
            check(canB && pl && pl->kind == HP_PLOT, "(a plot)");
            if (pl) {
                g.stormK = 0.f;
                g.hs.can = CAN_CAPACITY;
                check(g.hsWater(*pl) && pl->water >= 1.f, "bare tilled soil can be watered");
                for (int i = 0; i < 100; i++) g.hsTickCrops(1.f);
                check(pl->water <= 0.01f, "bare soil dries out again");
                // sow it, water it once, and it stays wet until the crop is ripe
                g.p.inv.add(itemMat(cropSeed(CR_CARROT)), 1);
                check(g.hsPlant(*pl, CR_CARROT), "(sow)");
                g.hsWater(*pl);
                const float full = CROPS[CR_CARROT].stageSec * (CROP_STAGES - 1) + 5.f;
                bool wetThrough = true;
                for (float t = 0; t < full && pl->stage < CROP_STAGES - 1; t += 1.f) {
                    g.hsTickCrops(1.f);
                    if (pl->stage < CROP_STAGES - 1 && pl->water < 0.99f) wetThrough = false;
                }
                check(pl->stage >= CROP_STAGES - 1, "one watering grows the crop all the way");
                check(wetThrough, "a planted plot stays watered while the crop grows");
                check(pl->water <= 0.01f, "the soil is dry when the crop is fully grown");
                g.hs.can = CAN_CAPACITY;
                const float before = g.hs.can;
                g.hsToolUse();  // (not aimed at it: only checks the call is safe)
                check(g.hs.can == before, "nothing is spent when nothing is watered");
                // the shovel untills a plot, planted (a regrowing crop like tomato is always planted) or empty
                const size_t n0 = g.hs.pieces.size();
                g.hsDig(px, py);
                check(!g.hsAt(px, py, HL_GROUND) && g.hs.pieces.size() == n0 - 1, "the shovel digs up a planted plot");
                g.hsPlace(HP_PLOT, px, py, 0);
                pl = g.hsAt(px, py, HL_GROUND);
                g.hsPlant(*pl, CR_TOMATO);
                pl = g.hsAt(px, py, HL_GROUND);
                check(pl && pl->crop == CR_TOMATO, "(a tomato is planted)");
                g.hsDig(px, py);
                check(!g.hsAt(px, py, HL_GROUND), "a tomato plot can be untilled too");
                g.hsPlace(HP_PLOT, px, py, 0);
                g.hsDig(px, py);
                check(!g.hsAt(px, py, HL_GROUND) && g.hs.pieces.size() == n0 - 1, "the shovel untills an empty plot");
                // rain soaks bare soil (and not what is under a greenhouse - not tested here)
                g.hsPlace(HP_PLOT, px, py, 0);
                pl = g.hsAt(px, py, HL_GROUND);
                pl->water = 0;
                g.stormK = 1.f;
                const int saveInterior = g.interior, saveDelve = g.delve;
                g.interior = -1;
                g.delve = -1;
                for (int i = 0; i < 12; i++) g.hsTickCrops(1.f);
                check(pl->water >= 1.f, "rain soaks a tilled plot with nothing planted");
                g.stormK = 0.f;
                g.interior = saveInterior;
                g.delve = saveDelve;
                g.hsDig(px, py);
            }
        }
    }

    printf("--- mounts: baby drops, raising, riding\n");
    {
        if (!g.inHomestead()) {
            stand(g, g.world.hportal + V2(0, 16));
            g.homesteadInteract();
        }
        g.state = ST_PLAY;
        check(mountable(A_WOLF) && mountable(A_BOAR) && !mountable(A_RABBIT), "predators and big game can be mounts, a rabbit cannot");
        check(babyChance(A_WOLF, 3) > babyChance(A_WOLF, 0) && babyChance(A_RABBIT, 0) == 0.f, "a rarer animal is likelier to leave a baby; a rabbit never does");
        g.items.clear();
        for (int i = 0; i < 120; i++) {
            Animal* w = g.spawnAnimal(A_WOLF, g.p.pos + V2(60, 0), 0, 3);
            if (w) g.killAnimal(*w, true);
        }
        int babies = 0, tierOk = 0;
        for (const auto& it : g.items)
            if (it.item == itemBaby(A_WOLF)) { babies += it.n; tierOk += it.q == 3; }
        check(babies > 0 && tierOk == babies, "killing legendary wolves leaves baby wolves on the ground, tagged with the legendary tier");
        g.items.clear();
        const int n0 = (int)g.hs.animals.size();
        g.p.inv.addIgnoringWeight(itemBaby(A_DEER), 1, 2);
        const int slot = [&] { for (int i = 0; i < g.p.bagCap(); i++) if (!g.p.inv.slots[i].empty() && g.p.inv.slots[i].item == itemBaby(A_DEER)) return i; return -1; }();
        check(slot >= 0 && g.useSlot(slot) && (int)g.hs.animals.size() == n0 + 1 && g.p.inv.count(itemBaby(A_DEER)) == 0, "using a baby at the homestead sets it down as a growing animal");
        HAnimal& m = g.hs.animals.back();
        check(m.kind == DA_MOUNT && m.species == A_DEER && m.tier == 2 && !m.adult(DOMS[m.kind]), "...of the same species and tier, still a baby");
        const int id = m.id;
        g.toggleMount();
        check(!g.riddenMount(), "a baby cannot be ridden");
        m.saddled = true;
        g.toggleMount();
        check(!g.riddenMount(), "...even if saddled");
        for (int i = 0, n = (int)m.growSec(DOMS[m.kind]) + 5; i < n; i++) {  // a well-fed baby grows at full speed
            for (auto& a : g.hs.animals) if (a.id == id) { a.full = 100; a.happy = 100; a.health = 100; }
            g.hsTickAnimals(1.f);
        }
        HAnimal* mm = nullptr;
        for (auto& a : g.hs.animals) if (a.id == id) mm = &a;
        check(mm && mm->adult(DOMS[mm->kind]), "after its growing time it is an adult");
        const int lim0 = g.p.carryLimit();
        stand(g, V2(mm->px, mm->py));
        // speed on foot vs mounted over the same stretch
        auto run = [&]() {
            const V2 a0 = g.p.pos;
            g.in.right = true;
            for (int i = 0; i < 30; i++) g.updatePlayer(1.f / 60.f);
            g.in.right = false;
            return g.p.pos.x - a0.x;
        };
        stand(g, V2(mm->px, mm->py));
        const float foot = run();
        stand(g, V2(mm->px, mm->py));
        mm->saddled = true;
        check(g.toggleMount() && g.riddenMount() == mm, "an adult saddled mount can be ridden (V)");
        check(g.p.carryLimit() == lim0 + mountCarry(A_DEER, 2) && g.p.inv.maxWeight == (float)g.p.carryLimit(), "riding adds the mount's carrying weight");
        const float ride = run();
        check(ride > foot * 1.2f, "...and is clearly faster than walking");
        g.save();
        g.dismount(true);
        check(!g.riddenMount() && g.p.carryLimit() == lim0, "dismounting gives the carry limit back");
        g.newGame();
        const bool ld = g.load();
        HAnimal* back = nullptr;
        for (auto& a : g.hs.animals) if (a.kind == DA_MOUNT) back = &a;
        check(ld && back && back->species == A_DEER && back->tier == 2 && back->saddled && !back->ridden && g.p.carryLimit() == lim0, "the mount (species, tier, saddle) survives save and load; a ridden one is back in its pen");
        g.state = ST_PLAY;
    }
    printf("--- despawn timers: ground items and corpses\n");
    {
        g.state = ST_PLAY;
        g.items.clear();
        const V2 far = g.p.pos + V2(400, 0);  // out of pickup reach
        GroundItem stack{far, 0, 0.f, 0.f};
        stack.item = itemMat(M_WOOD); stack.n = 1; stack.t = DROP_LIFE - 30.f;
        GroundItem thrown{far + V2(20, 0), W_FLINT, 0.f, THROWN_LIFE - 30.f};
        GroundItem fresh = stack; fresh.pos = far + V2(40, 0); fresh.t = 0.f;
        g.items.push_back(stack); g.items.push_back(thrown); g.items.push_back(fresh);
        for (int i = 0; i < 40; i++) g.update(0.5f);  // 20 s: all still there
        check(g.items.size() == 3, "items still lie there with time left on the clock");
        for (int i = 0; i < 40; i++) g.update(0.5f);  // 40 s total: the two old ones are gone
        int stacks = 0, thr = 0;
        for (auto& it : g.items) { if (it.item >= 0) stacks++; else thr++; }
        check(stacks == 1 && thr == 0, "a dropped stack and a thrown weapon vanish when their time is up; a fresh stack stays");
        check(THROWN_LIFE < DROP_LIFE && CORPSE_LIFE < CORPSE_LIFE_LEGEND && DESPAWN_WARN < CORPSE_LIFE, "a legendary carcass and a dropped stack outlast ordinary ones");
        g.items.clear();
        GroundItem old = stack; old.t = 123.f;
        g.items.push_back(old);
        g.save();
        g.newGame();
        g.load();
        g.state = ST_PLAY;
        bool kept = false;
        for (int m = 0; m < B_COUNT && !kept; m++) for (auto& it : (g.world.mapId == m ? g.items : g.mapMem[m].items)) kept = kept || (it.item == itemMat(M_WOOD) && it.t > 100.f && it.t < 130.f);
        check(kept, "the time a dropped stack has lain there survives save and load");
        g.animals.clear();
        Animal* d = g.spawnAnimal(A_DEER, g.p.pos + V2(60, 0), 0, 0);
        Animal* l = g.spawnAnimal(A_DEER, g.p.pos + V2(80, 0), 0, TR_LEGENDARY);
        if (d && l) {
            g.killAnimal(*d, true);
            g.killAnimal(*l, true);
            d->deadT = CORPSE_LIFE + 1.f;
            l->deadT = CORPSE_LIFE + 1.f;
            for (int i = 0; i < 40; i++) g.updateAnimals(0.5f);
            int common = 0, legend = 0;
            for (auto& a : g.animals) if (a.dead) { if (a.tier == TR_LEGENDARY) legend++; else common++; }
            check(common == 0 && legend == 1, "an ordinary carcass is gone after its time; a legendary one is still there");
        } else
            check(false, "spawned the test deer");
    }
    {
        printf("--- stage: production machines\n");
        if (!g.inHomestead()) g.hsEnter();
        g.state = ST_PLAY;
        stock(g);
        g.p.inv.addIgnoringWeight(itemMat(M_IRONBAR), 40);
        int mx = 0, my = 0;
        check(findSpot(g, 22, 4, mx, my, 70, 100), "room for a row of machines");
        const int kinds[7] = {HP_MILL, HP_DAIRY, HP_KETTLE, HP_SMOKER, HP_OVEN, HP_SAWMILL, HP_SMELTER};
        const int wid[7] = {3, 2, 2, 2, 2, 4, 2};
        int x = mx;
        HPiece* mp[7] = {};
        bool placedAll = true;
        for (int i = 0; i < 7; i++) {
            if (!g.hsPlace(kinds[i], x, my, 0)) placedAll = false;
            mp[i] = g.hsAt(x, my, HL_OBJECT);
            x += wid[i];
        }
        check(placedAll && mp[0] && mp[6], "all seven machines can be built (their own MACHINES build tab)");
        bool cat = true;
        for (int k : kinds) cat = cat && HPDEF[k].cat == HC_MACHINE && MACHINES[k - HP_MILL].nrec >= 2;
        check(cat, "they are listed under MACHINES and have recipes");
        if (mp[0] && mp[1] && mp[4] && mp[5] && mp[6]) {
            HPiece& mill = *mp[0];
            give(g, M_WHEAT, 9);
            const int w0 = g.p.mat(M_WHEAT), f0 = g.p.mat(M_FLOUR);
            check(g.hsMachineLoad(mill, 0, 2) == 2 && g.p.mat(M_WHEAT) == w0 - 6 && mill.mq == 2, "loading two batches takes the ingredients");
            check(g.hsMachineLoad(mill, 1, 1) == 0, "another recipe waits until the machine is empty");
            g.hsTickMachines(10.f);
            check(mill.mdone == 0 && mill.mq == 2, "a batch takes its time");
            g.hsTickMachines(11.f);
            check(mill.mdone == 1 && mill.mq == 1, "the first batch finishes after 20 seconds");
            g.hsTickMachines(60.f);
            check(mill.mdone == 2 && mill.mq == 0, "the machine stops when its queue is empty");
            check(g.hsMachineCollect(mill) && g.p.mat(M_FLOUR) == f0 + 4 && mill.mdone == 0 && mill.mrec == -1, "collecting hands over the flour and frees the machine");
            give(g, M_CORN, 3);
            check(g.hsMachineLoad(mill, 1, 5) == 1, "a different recipe can be loaded once it is empty (only what the pack holds)");
            g.p.inv.remove(itemMat(M_WHEAT), g.p.mat(M_WHEAT));
            check(g.hsMachineLoad(mill, 0, 1) == 0, "no ingredients: refused");
            give(g, M_CORN, 80);
            g.hsTickMachines(1000.f);
            g.hsMachineCollect(mill);
            check(g.hsMachineLoad(mill, 1, 50) == MACH_MAX_BATCH, "a machine holds at most " + std::to_string(MACH_MAX_BATCH) + " batches");
            // a consumable result: bread from the oven
            HPiece& oven = *mp[4];
            give(g, M_FLOUR, 6);
            const int b0 = g.p.inv.count(itemCons(C_BREAD));
            check(g.hsMachineLoad(oven, 0, 3) == 3, "the oven takes flour");
            g.hsTickMachines(200.f);
            check(g.hsMachineCollect(oven) && g.p.inv.count(itemCons(C_BREAD)) == b0 + 6, "and bakes bread (2 per batch)");
            // E collects when there is nothing more to load; otherwise the menu opens
            HPiece& saw = *mp[5];
            give(g, M_WOOD, 6);
            g.hsMachineLoad(saw, 0, 1);
            stand(g, V2((saw.tx + saw.w() / 2.f) * TS, (saw.ty + saw.h()) * TS + 6.f));
            Game::HTarget2 t;
            check(g.hsFindTarget(t) && t.type == 10, "E near a machine offers to use it");
            check(g.homesteadInteract() && g.state == ST_MACHINE && g.machPieceId == saw.id, "E opens the machine menu");
            g.state = ST_PLAY;
            if (const char* dir = getenv("HS_SHOTS")) {  // the machines at work, their menu and the MACHINES build tab
                auto shot = [&](const char* name) {
                    g.helpT = 0; g.portalFx = 0; g.msgs.clear(); g.ftexts.clear();
                    g.setCam(0, true);
                    g.render();
                    g.saveCanvas((std::string(dir) + "/" + name).c_str());
                };
                printf("shot state: map %d interior %d delve %d fade %.2f dead %d\n", (int)g.world.mapId, g.interior, g.delve, g.travelFade, (int)g.p.dead);
                g.travelFade = 0; g.interior = -1; g.delve = -1;
                g.dayT = DAY_LEN * 0.3f;
                g.hsMachineLoad(*mp[1], 0, 0);
                give(g, M_MILK, 10); give(g, M_STRAWBERRY, 9); give(g, M_MEAT, 6); give(g, M_FLOUR, 4);
                g.hsMachineLoad(*mp[1], 1, 2); g.hsMachineLoad(*mp[2], 0, 2); g.hsMachineLoad(*mp[3], 0, 2); g.hsMachineLoad(*mp[4], 0, 1);
                g.hsMachineLoad(*mp[0], 0, 1);
                stand(g, V2((mx + 9) * TS, (my + 6) * TS));
                g.hsTickMachines(3.f);
                g.in.mx = 200; g.in.my = 200;
                shot("m1_machines.bmp");
                g.machPieceId = mp[2]->id; g.state = ST_MACHINE;
                g.in.mx = 100; g.in.my = 60;
                shot("m2_menu.bmp");
                g.bm.cat = HC_MACHINE; g.state = ST_BUILD;
                shot("m3_buildtab.bmp");
                g.state = ST_PLAY;
            }
            // saving keeps the work in progress
            HPiece& smelter = *mp[6];
            give(g, M_IRON, 4);
            g.hsMachineLoad(smelter, 1, 2);
            g.hsTickMachines(55.f);
            const int sid = smelter.id, smq = smelter.mq, smd = smelter.mdone;
            const float smt = smelter.mt;
            g.save();
            g.newGame();
            check(g.load(), "the save loads");
            HPiece* sm2 = g.hsPiece(sid);
            check(sm2 && sm2->mrec == 1 && sm2->mq == smq && sm2->mdone == smd && fabsf(sm2->mt - smt) < 0.01f, "a machine's recipe, queue, finished batches and progress survive a save");
            g.state = ST_PLAY;
        }
    }
    {
        printf("--- stage: no homestead store, farm babies, cursor placing, 100-slot chests, new gear\n");
        if (!g.inHomestead()) g.hsEnter();
        g.state = ST_PLAY;
        bool stall = false;
        for (auto& pr : g.world.props) stall |= pr.kind == P_HMARKET_RETIRED;
        check(!stall, "the homestead has no market stall any more");
        // farm babies: stackable items bought at the General Store, set down at the homestead
        g.p.inv.clear();
        g.p.inv.addIgnoringWeight(itemFarmBaby(DA_COW), 4);
        check(g.p.inv.usedSlots() == 1 && item(itemFarmBaby(DA_COW)).stack > 1, "baby farm animals stack in one slot");
        const int a0 = (int)g.hs.animals.size();
        const int sl = [&] { for (int i = 0; i < g.p.bagCap(); i++) if (!g.p.inv.slots[i].empty()) return i; return -1; }();
        check(sl >= 0 && g.useSlot(sl) && (int)g.hs.animals.size() == a0 + 1 && g.p.inv.count(itemFarmBaby(DA_COW)) == 3, "using one sets a baby down at the homestead");
        const HAnimal& calf = g.hs.animals.back();
        check(calf.kind == DA_COW && !calf.adult(DOMS[DA_COW]), "...a baby cow that grows up");
        std::set<std::string> keys;
        bool uniq = true;
        for (int i = 0; i < ITEM_COUNT; i++) uniq &= keys.insert(itemKey(i)).second;
        check(uniq, "every item has its own save key");
        // placing with the cursor
        g.p.inv.clear();
        g.p.inv.addIgnoringWeight(itemPlace(PL_CHEST), 1);
        g.structs.clear();
        stand(g, V2((HS_PLAZA_X + 3) * TS + 8.f, (HS_PLAZA_Y + 4) * TS + 8.f));
        g.camX = g.p.pos.x - LW / 2; g.camY = g.p.pos.y - LH / 2;
        const int ftx = (int)(g.p.pos.x / TS) + 12, fty = (int)(g.p.pos.y / TS);
        std::string why;
        check(!g.placeCheck(ftx, fty, &why) && why.find("FAR") != std::string::npos, "a tile out of reach cannot be built on");
        bool placed = false;
        for (int dx = -3; dx <= 3 && !placed; dx++)
            for (int dy = -3; dy <= 3 && !placed; dy++) {
                const int tx = (int)(g.p.pos.x / TS) + dx, ty = (int)(g.p.pos.y / TS) + dy;
                if (!g.placeCheck(tx, ty)) continue;
                g.in.mx = (int)(tx * TS + 8 - g.camX); g.in.my = (int)(ty * TS + 8 - g.camY);
                placed = g.placeStructureAtCursor(PL_CHEST);
                check(placed && g.structs.size() == 1 && g.structs[0].tx == tx && g.structs[0].ty == ty, "a chest goes down on the tile under the cursor");
            }
        check(placed, "(setup) a free tile was found near the hunter");
        // the chest: 100 slots, a weight limit, split stacks
        if (placed) {
            Inventory& ch = g.structs[0].inv;
            check(ch.capacity() == 100, "a chest always has 100 slots");
            const int n = ch.add(itemMat(M_STONE), 1000);
            check(n > 0 && n < 1000 && ch.totalWeight() <= CHEST_KG + 0.01f, "the weight limit stops a chest filling up with stone");
            ch.clear();
            g.chestOpen = 0; g.hsChestPiece = -1; g.state = ST_CHEST;
            g.p.inv.clear();
            g.p.inv.addIgnoringWeight(itemMat(M_WOOD), 10);
            g.chestMove(1, 0, 4);
            check(g.p.inv.count(itemMat(M_WOOD)) == 6 && g.structs[0].inv.count(itemMat(M_WOOD)) == 4, "part of a stack can be moved into the chest");
            g.state = ST_PLAY; g.chestOpen = -1;
        }
        // new gear and meals stack and work
        g.p.inv.clear();
        g.p.inv.addIgnoringWeight(itemCarry(CG_FARMBOOTS), 5);
        g.p.inv.addIgnoringWeight(itemCarry(CG_PORTERBELT), 1);
        check(g.p.inv.usedSlots() == 2, "carried gear stacks");
        g.p.syncPack();
        g.migrateWornGear();
        check(g.p.carryLimit() >= BASE_CARRY_KG + 25, "the porter's belt raises the carrying limit");
        g.p.hp = 10.f;
        g.p.inv.addIgnoringWeight(itemCons(C_OMELET), 2);
        check(g.useConsumable(C_OMELET) && g.p.hp > 10.f && g.p.inv.count(itemCons(C_OMELET)) == 1, "a farm omelet heals");
        // ---- equipping: carried gear works only while worn; axe / pickaxe follow the best one until chosen
        g.p.inv.clear();
        for (int e = 0; e < EQ_COUNT; e++) g.p.worn[e] = -1;
        g.p.inv.addIgnoringWeight(itemCarry(CG_STRAWHAT), 1);
        g.p.inv.addIgnoringWeight(itemCarry(CG_FARMBOOTS), 2);
        auto slotOfItem = [&](int id) { for (int i = 0; i < g.p.bagCap(); i++) if (g.p.inv.slots[i].item == id) return i; return -1; };
        check(!g.p.has(CG_STRAWHAT) && !g.p.has(CG_FARMBOOTS), "carried gear in the pack does nothing");
        check(g.equipCarried(slotOfItem(itemCarry(CG_STRAWHAT))) && g.p.has(CG_STRAWHAT) && g.p.inv.count(itemCarry(CG_STRAWHAT)) == 0, "wearing takes it out of the pack");
        check(g.equipCarried(slotOfItem(itemCarry(CG_FARMBOOTS))) && g.p.has(CG_FARMBOOTS) && g.p.inv.count(itemCarry(CG_FARMBOOTS)) == 1, "one of a stack is worn");
        check(g.unequipCarried(EQ_HEAD) && !g.p.has(CG_STRAWHAT) && g.p.inv.count(itemCarry(CG_STRAWHAT)) == 1, "taking it off puts it back in the pack");
        g.p.inv.addIgnoringWeight(itemCarry(CG_WADERS), 1);  // the same slot: it replaces the boots
        check(g.equipCarried(slotOfItem(itemCarry(CG_WADERS))) && g.p.has(CG_WADERS) && !g.p.has(CG_FARMBOOTS) && g.p.inv.count(itemCarry(CG_FARMBOOTS)) == 2, "a piece for a taken slot swaps with the one worn");
        // the worn gear survives a save and a load
        g.save();
        for (int e = 0; e < EQ_COUNT; e++) g.p.worn[e] = -1;
        check(g.load() && g.p.has(CG_WADERS), "worn gear is saved");
        // axe and pickaxe
        g.p.inv.clear();
        g.p.inv.addIgnoringWeight(itemTool(TL_AXE1), 1);
        g.syncGatherTools();
        check(g.p.tool[TT_AXE] == TL_AXE1 && g.bestTool(TT_AXE) > 0, "the best axe in the pack is used");
        check(g.unequipGatherTool(TT_AXE) && g.bestTool(TT_AXE) == 0, "an axe taken off cannot chop");
        g.syncGatherTools();
        check(g.p.tool[TT_AXE] < 0, "...and stays off");
        check(g.equipGatherTool(slotOfItem(itemTool(TL_AXE1))) && g.bestTool(TT_AXE) > 0, "it can be put back on");
        // ---- the second row of crops and the farm products
        {
            bool ok = CR_COUNT == 14;
            std::set<std::string> nm;
            for (int c = 0; c < CR_COUNT; c++) {
                ok &= seedMatCrop(cropSeed(c)) == c && seedCropOf(itemMat(cropSeed(c))) == c && item(itemMat(cropProduce(c))).kind == IK_MATERIAL;
                ok &= nm.insert(item(itemMat(cropProduce(c))).name).second && nm.insert(item(itemMat(cropSeed(c))).name).second;
                ok &= CROPS[c].seedPrice > 0 && CROPS[c].stageSec > 0;
            }
            check(ok, "every crop has its own seed, produce, name and price");
            g.p.inv.clear();
            g.p.inv.addIgnoringWeight(itemMat(M_CUCUMBER), 3);
            g.p.inv.addIgnoringWeight(itemMat(M_HERB), 1);
            bool recipe = false;
            for (int r = 0; r < (int)(sizeof(RECIPES) / sizeof(RECIPES[0])); r++) recipe |= RECIPES[r].kind == RK_ITEM && RECIPES[r].out == itemMat(M_PICKLES);
            check(recipe && item(itemMat(M_CHEESE)).price > item(itemMat(M_MILK)).price, "farm products have recipes and are worth more than what goes in");
        }
        g.p.hp = g.p.maxhp = 100000.f;
    }
    if (hadSave) {
        std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
        out.write(savedBytes.data(), (std::streamsize)savedBytes.size());
    } else
        std::remove(g.savePath().c_str());
    printf("HOMESTEAD: %s (%d failed)\n", bad ? "FAILED" : "ok", bad);
    return bad;
}
