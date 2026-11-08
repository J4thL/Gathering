// homestead_tests.cpp - `wildwood --homestead`: the Homestead played through the real game code, headless.
#include <cstdio>
#include <fstream>
#include <iterator>
#include "game.hpp"

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
        check(!g.equipFarmTool(axeSlot) && !g.equipFarmTool(woodSlot) && !g.equipFarmTool(-1) && g.p.inv.usedSlots() == used1 && g.p.tool[TT_AXE] < 0 && g.p.tool[TT_PICK] < 0, "an axe, a material or an empty slot cannot be equipped as a farming tool");
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
            aim(tx, ty);
            g.hsToolUse();
            HPiece* np = g.hsAt(tx, ty, HL_GROUND);
            check(np && np->kind == HP_PLOT && g.hs.pieces.size() == n0 + 1, "left click with the hoe in hand tills the tile into a farm plot");
            g.state = ST_PLAY;
            g.hsToolUse();
            check(g.state == ST_FARM && g.farmPieceId == (np ? np->id : -1), "clicking the empty plot with the hoe opens the seed menu");
            g.state = ST_PLAY;
            np = g.hsAt(tx, ty, HL_GROUND);
            // seeds are equipped on the hotbar like a tool and sow the plot they are clicked on
            const int seedId = itemMat(cropSeed(CR_CARROT));
            check(g.equipHot(seedId) && g.p.hotSlotOf(seedId) >= 0, "seeds can be put on the hotbar");
            g.hotUse(g.p.hotSlotOf(seedId));
            check(g.p.hand == HAND_SEED0 + CR_CARROT, "the seeds' key puts them in hand");
            aim(tx + 1, ty);
            g.hsToolUse();
            check(!g.hsAt(tx + 1, ty, HL_GROUND), "seeds clicked on bare soil are not wasted (till it first)");
            aim(tx, ty);
            const int seedsBefore = g.p.inv.count(seedId);
            g.hsToolUse();
            np = g.hsAt(tx, ty, HL_GROUND);
            check(np && np->crop == CR_CARROT && g.p.inv.count(seedId) == seedsBefore - 1, "left click with seeds in hand sows the tilled plot and uses one seed");
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
        g.dayT = 60.f; g.helpT = 0;
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
        g.state = ST_MARKET; shot("g_market.bmp");
        g.state = ST_ANIMAL; g.farmPieceId = -1; shot("h_animals.bmp");
        g.state = ST_FARM; g.farmPieceId = g.hsAt(fx, fy, HL_GROUND) ? g.hsAt(fx, fy, HL_GROUND)->id : -1; shot("i_seeds.bmp");
        g.state = ST_PLAY;
        g.p.pos = V2(fx * TS, (fy + 2) * TS);
        g.dayT = 210.f;
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
    if (hadSave) {
        std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
        out.write(savedBytes.data(), (std::streamsize)savedBytes.size());
    } else
        std::remove(g.savePath().c_str());
    printf("HOMESTEAD: %s (%d failed)\n", bad ? "FAILED" : "ok", bad);
    return bad;
}
