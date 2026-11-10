// selftest_world.cpp - `--selftest`: gathering, structures, consumables, gear, maps and travel.
#include "tests/selftest_ctx.hpp"

void testWorldRules(SelfTest& t) {
    [[maybe_unused]] Game& g = t.g;
    [[maybe_unused]] auto check = [&](bool ok, const char* what) { t.check(ok, what); };
    [[maybe_unused]] const int MEAT = t.MEAT, HIDE = t.HIDE, BONE = t.BONE;
    [[maybe_unused]] const int R_BONE = t.R_BONE, R_ARROW = t.R_ARROW, R_BOLT = t.R_BOLT, R_BANDAGE = t.R_BANDAGE;
    [[maybe_unused]] auto fillWithJunk = [&]() { t.fillWithJunk(); };
    [[maybe_unused]] auto findRecipe = [&](int kind, int out, int firstMat) { return t.findRecipe(kind, out, firstMat); };
    [[maybe_unused]] auto writeFile = [&](const std::string& text) { t.writeFile(text); };
    [[maybe_unused]] auto removeAll = [&]() { t.removeAll(); };
    printf("gathering, crafting and structures\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.helpT = 0;
        g.p.inv.clear();
        auto recipeFor = [&](int kind, int out, int firstMat, int count = -1) {
            for (int r = 0; r < RECIPE_N; r++)
                if (RECIPES[r].kind == kind && RECIPES[r].out == out && RECIPES[r].mat[0] == firstMat && (count < 0 || RECIPES[r].count == count)) return r;
            return -1;
        };
        // stand beside resource `rs` such that findHarvest picks exactly it
        auto standAt = [&](int rs, bool& node, int& idx, int skip = 0) {
            for (int pass = 0; pass < 2; pass++) {
                const bool wantNode = pass == 0;
                const size_t n = wantNode ? g.world.nodes.size() : g.world.props.size();
                for (size_t i = 0; i < n; i++) {
                    const int res = wantNode ? nodeRes(g.world.nodes[i].kind) : g.propRsId[i];
                    if (res != rs) continue;
                    const float x = wantNode ? g.world.nodes[i].x : g.world.props[i].x, y = wantNode ? g.world.nodes[i].y : g.world.props[i].y;
                    g.p.pos = V2(x, y + 12);
                    HTarget t;
                    if (g.findHarvest(t) && t.node == wantNode && t.idx == (int)i && skip-- <= 0) {
                        node = wantNode;
                        idx = (int)i;
                        return true;
                    }
                }
            }
            return false;
        };
        auto swingUntilGone = [&](bool node, int idx, int& steps) {
            steps = 0;
            while (!g.hstate(node, idx).gone && steps < 4000) {
                HTarget t;
                if (!g.findHarvest(t) || t.idx != idx || t.node != node) return false;
                g.harvestStep(t, 0.05f);
                steps++;
            }
            return g.hstate(node, idx).gone;
        };
        auto canHarvest = [&](int rs) {
            const ResDef& RD = RES[rs];
            return RD.tool < 0 || g.bestTool(RD.tool) >= RD.minTier;
        };
        bool node = false;
        int idx = -1, steps = 0;

        // ---- tool tiers gate ore (iron is found in the mountain map)
        g.travelTo(B_MOUNTAIN, MAPS[B_MOUNTAIN].entryTx, MAPS[B_MOUNTAIN].entryTy, false);
        g.travelCd = 0;
        check(standAt(RS_IRON, node, idx), "an iron vein exists and can be reached by line of sight");
        check(!canHarvest(RS_IRON), "iron cannot be mined with bare hands");
        g.p.inv.add(itemTool(TL_PICK1), 1);
        check(!canHarvest(RS_IRON), "a stone pickaxe (tier 1) is not enough for iron");
        g.p.inv.add(itemTool(TL_PICK2), 1);
        check(canHarvest(RS_IRON) && !canHarvest(RS_GOLD), "a copper pickaxe mines iron but not gold");
        check(g.bestTool(TT_PICK) == 2 && g.bestTool(TT_AXE) == 0, "bestTool picks the highest tier of the right type");
        // ---- swing, finish, reward, depletion
        const int ironBefore = g.p.mat(M_IRON);
        const bool done = swingUntilGone(node, idx, steps);
        check(done, "holding the swing finishes the vein");
        check(g.p.mat(M_IRON) > ironBefore, "finishing the vein pays out iron ore");
        check(g.p.sk.s[SK_MINING].xp == 2 + RES[RS_IRON].hits * 2 + RES[RS_IRON].minTier * 4, "finishing a vein earns Mining XP once, not per swing");
        const float minSecs = (RES[RS_IRON].hits - 1) * RES[RS_IRON].swing / TOOLDEF[TL_PICK2].speed;
        check(steps * 0.05f >= minSecs, "a vein takes at least hits x swing time (no free rewards)");
        {
            HTarget t;
            const bool still = g.findHarvest(t) && t.node == node && t.idx == idx;
            check(!still, "a depleted vein cannot be harvested again (no duplicate rewards)");
            const int had = g.p.mat(M_IRON);
            HTarget fake;
            fake.valid = true; fake.node = node; fake.idx = idx; fake.res = RS_IRON; fake.pos = g.p.pos;
            for (int i = 0; i < 200; i++) g.harvestStep(fake, 0.05f);
            check(g.p.mat(M_IRON) == had, "forcing swings at a depleted vein gives nothing");
        }
        // ---- respawn timing
        {
            for (float t = 0; t < RES[RS_IRON].respawn * 0.5f; t += 1.f) g.updateHarvest(1.f);
            check(g.hstate(node, idx).gone, "it is still depleted halfway through the respawn time");
            for (float t = 0; t < RES[RS_IRON].respawn * 0.7f + REGROW_FADE + 2.f; t += 1.f) g.updateHarvest(1.f);
            check(g.harvestUp(node, idx), "it has grown back after the respawn time");
            HTarget t;
            check(g.findHarvest(t) && t.idx == idx, "and can be harvested again");
        }
        g.travelTo(B_HOME, MAPS[B_MOUNTAIN].portals[0].arriveTx, MAPS[B_MOUNTAIN].portals[0].arriveTy, false);  // back to the home map for the trees and herbs
        g.travelCd = 0;
        // ---- trees: solid tile cleared while felled, restored when regrown, border trees stay
        {
            g.p.inv.add(itemTool(TL_AXE1), 1);
            check(standAt(RS_OAK, node, idx, 5), "an oak is reachable");
            const Prop& pr = g.world.props[idx];
            const int tx = (int)(pr.x / TS), ty = (int)(pr.y / TS);
            check(g.world.solid[ty][tx], "a standing oak is a solid tile");
            const int woodBefore = g.p.mat(M_WOOD);
            check(swingUntilGone(node, idx, steps), "an oak can be chopped down");
            check(g.p.impactT > 0, "the blow that lands starts the hunter's impact / recovery pose");
            check(g.p.mat(M_WOOD) > woodBefore, "felling an oak gives wood");
            check(g.p.sk.s[SK_WOODCUTTING].xp == 2 + RES[RS_OAK].hits * 2 + RES[RS_OAK].minTier * 4, "felling an oak earns Woodcutting XP once");
            check(!g.world.solid[ty][tx], "a felled tree no longer blocks the way");
            g.p.pos = V2(pr.x + 120, pr.y + 120);  // walk away: nobody blocks the regrowth
            for (float t = 0; t < RES[RS_OAK].respawn * 1.3f + REGROW_FADE + 5.f; t += 1.f) g.updateHarvest(1.f);
            check(g.harvestUp(node, idx) && g.world.solid[ty][tx], "the oak regrows and is solid again");
        }
        {
            int bad = 0, harvestable = 0;
            for (size_t i = 0; i < g.world.props.size(); i++) {
                const int tx = (int)(g.world.props[i].x / TS), ty = (int)(g.world.props[i].y / TS);
                if (g.propRsId[i] >= 0) {
                    harvestable++;
                    if (protectedTile(tx, ty, g.world.mapId)) bad++;
                }
            }
            check(harvestable > 0 && bad == 0, "trees on the map border and around the home core are never harvestable");
        }
        // ---- no gathering through obstacles
        {
            check(standAt(RS_HERB, node, idx), "a wild herb is reachable");
            const V2 target(g.world.nodes[idx].x, g.world.nodes[idx].y - 4);
            // put a solid wall tile between the hunter and the plant
            const int wx = (int)((g.p.pos.x + target.x) / 2 / TS), wy = (int)((g.p.pos.y + target.y) / 2 / TS);
            const bool had = g.world.solid[wy][wx];
            const int ptx = (int)(target.x / TS), pty = (int)(target.y / TS);
            if ((wx != ptx || wy != pty)) {
                g.world.solid[wy][wx] = true;
                g.p.pos = target + V2(0, 24);
                g.world.solid[(int)((g.p.pos.y + target.y) / 2 / TS)][(int)(target.x / TS)] = true;
                HTarget t;
                const bool seen = g.findHarvest(t) && t.node && t.idx == idx;
                check(!seen, "a solid tile between the hunter and the plant blocks gathering");
                g.world.solid[(int)((g.p.pos.y + target.y) / 2 / TS)][(int)(target.x / TS)] = had;
            }
            g.world.solid[wy][wx] = had;
        }
        // ---- a full bag never swallows a resource
        {
            check(standAt(RS_HERB, node, idx, 1), "another herb is reachable");
            g.p.inv.clear();
            g.p.inv.maxWeight = 1e6f;
            for (int i = 0; i < g.p.bagCap(); i++) g.p.inv.add(itemWpn(W_BOW), 1);
            check(g.p.bagUsed() == g.p.bagCap(), "test setup: the bag is full");
            for (int i = 0; i < 200; i++) {
                HTarget t;
                if (!g.findHarvest(t)) break;
                g.harvestStep(t, 0.05f);
            }
            check(!g.hstate(node, idx).gone && g.p.mat(M_HERB) == 0, "with a full bag the plant stays and nothing is lost");
            g.p.inv.clear();
            check(swingUntilGone(node, idx, steps) && g.p.mat(M_HERB) > 0, "with room again the same plant gives its reward");
        }
        // ---- crafting rules
        g.p.inv.clear();
        g.structs.clear();
        {
            // a spot well away from the village: nothing is "near" a fire or the shop there
            bool found = false;
            for (int ty = 20; ty < MH - 20 && !found; ty += 5)
                for (int tx = 20; tx < MW - 20 && !found; tx += 5) {
                    g.p.pos = V2(tx * TS + 8.f, ty * TS + 8.f - 18.f);
                    g.p.dir = 0;
                    if (dist(g.p.pos, g.world.fire) > 150 && dist(g.p.pos, g.world.keeper) > 150 && g.canPlaceAt(tx, ty) && !g.world.solidTile(tx, ty - 2)) found = true;
                }
            check(found, "test setup: an open spot far from the village");
            const int rCamp = recipeFor(RK_ITEM, itemPlace(PL_CAMPFIRE), M_WOOD);
            const int rCopperBar = recipeFor(RK_ITEM, itemMat(M_COPPERBAR), M_COPPER);
            const int rRope = recipeFor(RK_ITEM, itemMat(M_ROPE), M_FIBER);
            const int rCuAxe = recipeFor(RK_ITEM, itemTool(TL_AXE2), M_WOOD);
            const int rArmor1 = recipeFor(RK_GEAR, U_ARMOR, M_HIDE, 1), rArmor2 = recipeFor(RK_GEAR, U_ARMOR, M_FUR, 2);
            const int rIronAxe = recipeFor(RK_ITEM, itemTool(TL_AXE3), M_WOOD);
            check(rCamp >= 0 && rCopperBar >= 0 && rRope >= 0 && rCuAxe >= 0 && rArmor1 >= 0 && rArmor2 >= 0 && rIronAxe >= 0, "the key recipes are all in the table");
            check(g.craftBlock(rCamp) == "MISSING MATERIALS", "a recipe without its materials says so");
            // every recipe in the table produces something real and names real ingredients
            int badRecipes = 0;
            for (int r = 0; r < RECIPE_N; r++) {
                const Recipe& R = RECIPES[r];
                bool ok = R.count > 0 && !recipeName(R).empty();
                int ing = 0;
                for (int k = 0; k < 3; k++)
                    if (R.mat[k] >= 0) {
                        ing++;
                        ok = ok && R.mat[k] < M_COUNT && R.n[k] > 0;
                    }
                ok = ok && ing > 0;
                if (R.kind != RK_GEAR && R.kind != RK_BANDAGE) ok = ok && recipeItem(R) > 0 && recipeItem(R) < ITEM_COUNT;
                if (!ok) {
                    badRecipes++;
                    printf("    bad recipe #%d\n", r);
                }
            }
            check(badRecipes == 0, "every recipe has valid ingredients and a real output");
            // each raw material that a recipe needs can be gathered, dropped or made
            bool producible[M_COUNT] = {false};
            for (int rs = 0; rs < RS_COUNT; rs++)
                for (int k = 0; k < 3; k++)
                    if (RES[rs].y[k].mat >= 0) producible[RES[rs].y[k].mat] = true;
            for (int a = 0; a < A_COUNT; a++)
                for (int m = 0; m < M_COUNT; m++)
                    if (ANIMAL[a].drop[m][1] > 0) producible[m] = true;
            for (int k = 0; k < MK_COUNT; k++)  // the delve creatures' drops
                for (const MonDrop& d : MONDEF(k).drops)
                    if (d.chance > 0) producible[d.mat] = true;
            for (int r = 0; r < RECIPE_N; r++)
                if (RECIPES[r].kind == RK_ITEM && item(RECIPES[r].out).kind == IK_MATERIAL) producible[item(RECIPES[r].out).ref] = true;
            for (int m : {M_ROSE, M_TULIP, M_LAVENDER, M_SUNFLOWER}) producible[m] = true;  // bought at the flower shop
            int unmakeable = 0;
            for (int r = 0; r < RECIPE_N; r++)
                for (int k = 0; k < 3; k++)
                    if (RECIPES[r].mat[k] >= 0 && !producible[RECIPES[r].mat[k]]) {
                        unmakeable++;
                        printf("    recipe #%d needs %s, which nothing provides\n", r, MAT_NAME[RECIPES[r].mat[k]]);
                    }
            check(unmakeable == 0, "every ingredient of every recipe can actually be obtained");

            g.p.inv.add(itemMat(M_COPPER), 2);
            g.p.inv.add(itemMat(M_WOOD), 12);
            g.p.inv.add(itemMat(M_STONE), 6);
            g.craftAnywhere = false;
            check(g.craftBlock(rCopperBar) == "MADE AT THE BLACKSMITH", "smelting outdoors is refused: it is done at the blacksmith");
            check(!g.craft(rCopperBar) && g.p.mat(M_COPPER) == 2, "...and consumes nothing");
            g.interior = BLD_WORKSHOP;
            check(g.craftBlock(rCopperBar) == "MADE AT THE BLACKSMITH", "the wrong building refuses it too");
            g.interior = -1;
            g.craftAnywhere = true;
            check(g.craft(rCamp) && g.p.inv.count(itemPlace(PL_CAMPFIRE)) == 1 && g.p.mat(M_STONE) == 3, "a campfire is crafted from wood and stone");
            check(g.placeStructure(PL_CAMPFIRE) && g.structs.size() == 1 && g.p.inv.count(itemPlace(PL_CAMPFIRE)) == 0, "the campfire is placed");
            check(g.nearFire(), "standing by the placed campfire counts as a fire");
            g.craftAnywhere = false;
            g.interior = BLD_SMITH;
            check(g.craft(rCopperBar) && g.p.mat(M_COPPERBAR) == 1 && g.p.mat(M_COPPER) == 0, "copper smelts into a bar at the blacksmith");
            g.interior = -1;
            g.craftAnywhere = true;
            // rope, then the copper axe
            g.p.inv.add(itemMat(M_FIBER), 3);
            check(g.craft(rRope) && g.p.mat(M_ROPE) == 1 && g.p.mat(M_FIBER) == 0, "fibre twists into rope");
            check(g.p.mat(M_COPPERBAR) == 1, "(bar still in the bag)");
            g.p.inv.add(itemMat(M_COPPERBAR), 1);
            check(g.craft(rCuAxe) && g.p.inv.count(itemTool(TL_AXE2)) == 1, "a copper axe is crafted from wood, a copper bar and rope");
            g.p.inv.add(itemMat(M_COPPERBAR), 2);
            g.p.inv.add(itemMat(M_ROPE), 1);
            g.p.inv.add(itemMat(M_WOOD), 2);
            check(g.craftBlock(rCuAxe).empty(), "a second copy of a single tool can be crafted too (crafting has no one-per-item limit)");
            check(g.craftMany(rCuAxe, 5) == 1 && g.p.inv.count(itemTool(TL_AXE2)) == 2, "craft-many stops when the materials run out");
            g.p.inv.add(itemMat(M_ROPE), 1);  // (the later checks expect the materials this one used to leave alone)
            g.p.inv.add(itemMat(M_WOOD), 2);
            g.p.inv.add(itemMat(M_COPPERBAR), 2);
            // iron tools need the shop's forge
            g.p.inv.add(itemMat(M_IRONBAR), 2);
            g.craftAnywhere = false;
            check(g.craftBlock(rIronAxe) == "MADE AT THE BLACKSMITH", "iron tools are made at the blacksmith");
            g.craftAnywhere = true;
            // armor order
            g.p.inv.add(itemMat(M_FUR), 4);
            g.p.inv.add(itemMat(M_HIDE), 12);
            g.p.inv.add(itemMat(M_FIBER), 2);
            check(g.craftBlock(rArmor2) == "MAKE THE PREVIOUS LEVEL FIRST", "armor level 2 needs level 1 first");
            check(g.craft(rArmor1) && g.p.up[U_ARMOR] == 1, "hide armor is worn at once");
            check(g.craft(rArmor2) && g.p.up[U_ARMOR] == 2, "then the second level");
            check(g.craftBlock(rArmor1) == "YOU WEAR BETTER ALREADY" || g.craftBlock(rArmor1) == "MISSING MATERIALS", "an older armor level cannot be crafted again");
            // consumables
            g.p.inv.clear();
            g.p.hp = g.p.maxhp - 50;
            g.p.inv.add(itemCons(C_SALVE), 1);
            g.p.inv.add(itemCons(C_BREW), 1);
            g.p.inv.add(itemCons(C_CACTUSWATER), 1);
            const float h0 = g.p.hp;
            check(g.useConsumable(C_SALVE) && g.p.hp > h0 && g.p.inv.count(itemCons(C_SALVE)) == 0, "a salve heals and is used up");
            g.p.hp = g.p.maxhp;
            check(!g.useConsumable(C_TONIC), "using something you do not own does nothing");
            check(g.useConsumable(C_BREW) && g.p.warmT > 100.f, "a warm brew gives warmth");
            check(g.useConsumable(C_CACTUSWATER) && g.p.coolT > 60.f && g.p.stamina >= 99.f, "cactus water refreshes");
            // chest: move items both ways, keeps the stack rules
            g.p.inv.clear();
            g.p.inv.add(itemMat(M_WOOD), 5);
            g.p.inv.add(itemMat(M_WOOD), 0);
            g.p.inv.add(itemPlace(PL_CHEST), 1);
            check(g.placeStructure(PL_CHEST) && g.structs.size() == 2, "a chest is placed");
            g.chestOpen = 1;
            int woodSlot = -1;
            for (int i = 0; i < g.p.bagCap(); i++)
                if (g.p.inv.slots[i].item == itemMat(M_WOOD)) woodSlot = i;
            check(woodSlot >= 0 && g.chestMove(1, woodSlot) && g.p.mat(M_WOOD) == 0 && g.structs[1].inv.count(itemMat(M_WOOD)) == 5, "wood moves from the pack into the chest");
            // save / load: depleted resources, structures and chest contents come back
            check(standAt(RS_HERB, node, idx, 2) && swingUntilGone(node, idx, steps), "(setup) one more plant is picked");
            g.save();
            const size_t nStructs = g.structs.size();
            g.structs.clear();
            g.resetHarvest();
            check(g.harvestUp(node, idx), "(setup) after a reset the plant is back");
            check(g.load(), "the save loads");
            check(g.structs.size() == nStructs, "placed structures survive a save / load");
            bool chestOk = false;
            for (auto& s : g.structs)
                if (s.kind == PL_CHEST && s.inv.count(itemMat(M_WOOD)) == 5) chestOk = true;
            check(chestOk, "chest contents survive a save / load");
            check(g.hstate(node, idx).gone, "a picked plant is still waiting to regrow after a save / load");
        }
    }
    printf("consumables and skill-gated crafting\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.p.inv.clear();
        auto cons = [&](int c) { g.p.inv.add(itemCons(c), 1); };
        g.p.maxhp = 200;
        g.p.hp = 20;
        cons(C_POULTICE);
        check(g.useConsumable(C_POULTICE) && (int)g.p.hp == 80 && g.p.inv.count(itemCons(C_POULTICE)) == 0, "a poultice heals 60 and is used up");
        cons(C_MEDICINE);
        check(g.useConsumable(C_MEDICINE) && (int)g.p.hp == 200, "herbal medicine heals 120 (capped at max health)");
        cons(C_POULTICE);
        check(!g.useConsumable(C_POULTICE) && g.p.inv.count(itemCons(C_POULTICE)) == 1, "healing items are refused (and kept) at full health");
        g.p.hp = 100;
        g.p.stamina = 5.f;
        g.p.exhausted = true;
        cons(C_JERKY);
        check(g.useConsumable(C_JERKY) && (int)g.p.hp == 120 && g.p.stamina >= 99.f && !g.p.exhausted, "dried meat heals 20 and restores stamina");
        g.p.stamina = 5.f;
        cons(C_STEW);
        check(g.useConsumable(C_STEW) && (int)g.p.hp == 200 && g.p.stamina >= 99.f, "hunter's stew heals 85 and restores stamina");
        g.p.hp = 50;
        cons(C_VIGOR);
        check(g.useConsumable(C_VIGOR) && (int)g.p.hp == 105 && g.p.swiftT > 85.f, "velvet tonic heals 55 and grants a speed boost");
        cons(C_NIGHTSIGHT);
        check(g.useConsumable(C_NIGHTSIGHT) && g.p.nightT > 295.f, "nightsight tonic starts a five-minute timer");
        cons(C_NIGHTSIGHT);
        check(!g.useConsumable(C_NIGHTSIGHT) && g.p.inv.count(itemCons(C_NIGHTSIGHT)) == 1, "a second nightsight tonic is refused while one is active (nothing wasted)");
        // skill gating
        auto find = [&](int kind, int out, int firstMat, int count) {
            for (int r = 0; r < RECIPE_N; r++)
                if (RECIPES[r].kind == kind && RECIPES[r].out == out && RECIPES[r].mat[0] == firstMat && RECIPES[r].count == count) return r;
            return -1;
        };
        const int rPoul = find(RK_ITEM, itemCons(C_POULTICE), M_EXTRACT, 2);
        check(rPoul >= 0, "(setup) the poultice recipe exists");
        g.newGame();
        g.state = ST_PLAY;
        g.p.inv.clear();
        g.p.inv.add(itemMat(M_EXTRACT), 4);
        g.p.inv.add(itemMat(M_FIBER), 4);
        check(g.craftBlock(rPoul) == "NEEDS ALCHEMY 3", "a recipe above your skill level says which skill and level it needs");
        check(!g.craft(rPoul) && g.p.mat(M_EXTRACT) == 4 && g.p.mat(M_FIBER) == 4 && g.p.inv.count(itemCons(C_POULTICE)) == 0, "...and a locked recipe consumes nothing");
        g.p.sk.s[SK_ALCHEMY].xp = xpForLevel(1) + xpForLevel(2);  // exactly level 3
        check(g.p.sk.level(SK_ALCHEMY) == 3 && g.craftBlock(rPoul).empty(), "reaching the level unlocks it");
        const int64_t before = g.p.sk.s[SK_ALCHEMY].xp;
        check(g.craft(rPoul) && g.p.inv.count(itemCons(C_POULTICE)) == 2 && g.p.mat(M_EXTRACT) == 3 && g.p.mat(M_FIBER) == 3, "crafting delivers the result and spends the ingredients");
        check(g.p.sk.s[SK_ALCHEMY].xp == before + recipeXp(RECIPES[rPoul]), "a successful craft earns the recipe's XP in the recipe's skill");
        g.p.inv.clear();
        const int64_t b2 = g.p.sk.s[SK_ALCHEMY].xp;
        check(!g.craft(rPoul) && g.p.sk.s[SK_ALCHEMY].xp == b2, "a failed craft earns no XP");
    }
    printf("gear effects and recipe audit\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.p.inv.clear();
        float dr = 1, rg = 1;
        g.curBiome = B_DESERT;
        g.dayT = 0.5f;
        g.climateMods(dr, rg);
        const float plainHeat = dr;
        g.p.inv.add(itemCarry(CG_DESERTWRAPS), 1);
        g.climateMods(dr, rg);
        check(plainHeat > 1.f && dr < plainHeat && dr >= 1.f, "desert wraps cut the heat drain while carried");
        g.p.inv.clear();
        g.curBiome = B_SNOW;
        g.p.warmT = 0;
        g.climateMods(dr, rg);
        const float cold = dr;
        g.p.inv.add(itemCarry(CG_WOLFCLOAK), 1);
        g.climateMods(dr, rg);
        const float cloak = dr;
        g.p.inv.add(itemCarry(CG_PARKA), 1);
        g.climateMods(dr, rg);
        check(cold > cloak && cloak > dr && dr == 1.f, "the wolf cloak halves the cold and the parka removes it");
        g.p.inv.clear();
        g.curBiome = B_MOUNTAIN;
        g.climateMods(dr, rg);
        const float thin = dr;
        g.p.inv.add(itemCarry(CG_HARNESS), 1);
        g.climateMods(dr, rg);
        check(dr < thin, "the climbing harness eases thin air");
        // snake wraps: only snake bites are softened
        g.p.inv.clear();
        g.p.hp = g.p.maxhp = 200;
        g.hurtPlayer(20, V2(0, 0), A_RATTLER);
        const float plain = 200 - g.p.hp;
        g.p.hp = 200;
        g.p.invuln = 0;
        g.p.inv.add(itemCarry(CG_VENOMWRAPS), 1);
        g.hurtPlayer(20, V2(0, 0), A_RATTLER);
        const float wrapped = 200 - g.p.hp;
        g.p.hp = 200;
        g.p.invuln = 0;
        g.hurtPlayer(20, V2(0, 0), A_WOLF);
        check(wrapped < plain && 200 - g.p.hp == plain, "venom wraps soften snake bites but not other attacks");
        // gear tiers
        g.p.inv.clear();
        bool tiers = true;
        for (int u = 0; u < 4; u++) tiers = tiers && (u == U_BAG || std::string(gearName(u, UP_MAX[u])).size() > 0);  // bags are items now
        check(tiers, "every gear line's top level has a name");
        g.newGame();
        g.state = ST_PLAY;
        g.p.sk.s[SK_LEATHER].xp = SkillState::capXp();
        g.p.inv.add(itemMat(M_LEATHER), 8);
        g.p.inv.add(itemMat(M_ROPE), 4);
        g.p.inv.add(itemMat(M_MARMOTPELT), 3);
        int rPack = -1;
        for (int r = 0; r < RECIPE_N; r++)
            if (RECIPES[r].kind == RK_ITEM && RECIPES[r].out == itemPack(PK_EXPLORER)) rPack = r;
        g.p.sk.s[SK_LEATHER].xp = 0;
        check(rPack >= 0 && g.craftBlock(rPack).find("NEEDS LEATHERWORK") == 0, "the explorer pack needs leatherwork skill");
        g.p.sk.s[SK_LEATHER].xp = SkillState::capXp();
        g.p.pos = g.world.fire;  // the recipe needs a campfire
        check(g.craft(rPack) && g.p.inv.count(itemPack(PK_EXPLORER)) == 1 && g.p.bag < 0, "crafting the explorer pack makes a bag item (worn from the pack, not automatically)");
        check(g.craftBlock(rPack).find("MISSING") == 0 || g.craftBlock(rPack).find("ALREADY") != std::string::npos, "a second copy of a bag you already own is refused");
        // audit
        bool valid = true, skilled = true;
        bool used[M_COUNT] = {};
        for (int r = 0; r < RECIPE_N; r++) {
            const Recipe& R = RECIPES[r];
            valid = valid && R.count > 0 && !recipeName(R).empty();
            skilled = skilled && R.skill >= 0 && R.skill < SK_COUNT && R.minLv >= 1 && R.minLv <= MAX_LEVEL;
            for (int i = 0; i < 3; i++)
                if (R.mat[i] >= 0) {
                    valid = valid && R.mat[i] < M_COUNT && R.n[i] > 0;
                    used[R.mat[i]] = true;
                }
        }
        check(valid && skilled, "every recipe has a name, a count, valid ingredients and a valid skill level");
        bool trophiesUsed = true;
        for (int m = M_TROPHY0; m < M_TROPHY0 + A_COUNT; m++)
            if (!used[m]) {
                trophiesUsed = false;
                printf("    %s is used by no recipe\n", MAT_NAME[m]);
            }
        check(trophiesUsed, "every animal's exclusive material is an ingredient in at least one recipe");
        bool compUsed = true;
        for (int m : {M_PLANK, M_LEATHER, M_BONEFRAG, M_EXTRACT, M_GREASE, M_FAT})
            if (!used[m]) {
                compUsed = false;
                printf("    %s is used by no recipe\n", MAT_NAME[m]);
            }
        check(compUsed, "every processed material is consumed by a recipe");
        bool biomeOk = true;
        for (int w = W_FANGDAGGER; w < W_COUNT; w++) biomeOk = biomeOk && (WPN[w].biome < 0 || WPN[w].biomeMul > 1.f);
        check(biomeOk, "every biome weapon has a damage bonus for its biome");
    }
    printf("separate maps and travel\n");
    {
        auto packTotal = [&]() {
            long n = 0;
            for (auto& sl : g.p.inv.slots) n += sl.empty() ? 0 : sl.n;
            return n;
        };
        auto spotOk = [&]() {
            const V2 q = g.p.pos;
            return !g.world.blocked(q.x, q.y, 5.f) && q.x > 2 * TS && q.y > 2 * TS && q.x < (MW - 2) * TS && q.y < (MH - 2) * TS;
        };
        check(MW == 240 && MH == 168 && CW == 88 && CH == 60, "every map is 240x168 tiles; the original 88x60 Wildwood area sits inside the home map");
        bool allSame = true, allWalled = true, pure = true;
        for (int m = 0; m < B_COUNT; m++) {
            g.loadMapWorld(m);
            allSame = allSame && g.world.mapId == m && g.world.pixW() == 240 * TS && g.world.pixH() == 168 * TS;
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++) {
                    pure = pure && g.world.biome[y][x] == m;
                    if ((x < 2 || y < 2 || x >= MW - 2 || y >= MH - 2) && !g.world.solidTile(x, y) && !portalAt(m, x, y)) allWalled = false;
                }
        }
        check(allSame && pure, "all five maps have the same pixel size and only their own biome");
        check(allWalled, "every map's edge wall is closed except at its gates");
        g.loadMapWorld(B_HOME);
        bool homeSame = true;  // the same seed always builds the same map
        {
            World a, b;
            a.generate(WORLD_SEED, B_DESERT);
            b.generate(WORLD_SEED, B_DESERT);
            homeSame = a.props.size() == b.props.size() && a.nodes.size() == b.nodes.size() && memcmp(a.tile, b.tile, sizeof a.tile) == 0;
        }
        check(homeSame, "a map is rebuilt identically every time it is entered");
        // ---- travel keeps the hunter, and each map keeps its own state
        g.newGame();
        g.state = ST_PLAY;
        g.p.coins = 500;
        g.p.inv.add(itemMat(M_WOOD), 7);
        g.p.inv.add(itemMat(M_STONE), 5);
        g.p.inv.add(itemPlace(PL_CAMPFIRE), 2);
        g.p.hp = 55;
        g.p.sk.s[SK_HUNTING].xp = 300;
        g.questIdx = 2;
        g.dayT = 77;
        const long packBefore = packTotal();
        check(g.world.mapId == B_HOME && g.placeStructure(PL_CAMPFIRE) && g.structs.size() == 1, "(setup) a campfire is placed in the home map");
        const int homeTx = g.structs[0].tx, homeTy = g.structs[0].ty;
        g.p.inv.add(itemPlace(PL_CAMPFIRE), 0);
        const long packMid = packTotal();
        check(packMid <= packBefore, "(setup) placing a structure never adds items to the pack");
        check(g.travelTo(B_DESERT, MAPS[B_DESERT].entryTx, MAPS[B_DESERT].entryTy, false) && g.world.mapId == B_DESERT && g.curBiome == B_DESERT, "travelling loads the desert map");
        check(spotOk(), "the hunter arrives on a free tile inside the map");
        check(packTotal() == packMid && g.p.coins == 500 && (int)g.p.hp == 55 && g.p.sk.s[SK_HUNTING].xp == 300 && g.questIdx == 2 && g.dayT == 77.f, "pack, coins, health, skills, quest and time are unchanged by a trip");
        check(g.structs.empty(), "the home campfire is not in the desert (maps do not mix)");
        g.travelCd = 0;
        check(g.placeStructure(PL_CAMPFIRE) && g.structs.size() == 1, "(setup) a campfire is placed in the desert");
        const int dTx = g.structs[0].tx, dTy = g.structs[0].ty;
        const long packNow = packTotal();  // (the placed campfire left the pack)
        check(g.world.landmarks.size() == 4 && g.landmarksFound() == 0, "the desert has its own four landmarks, none found yet");
        g.world.landmarks[1].found = true;
        int herb = -1;
        for (size_t i = 0; i < g.world.nodes.size(); i++)
            if (nodeRes(g.world.nodes[i].kind) >= 0) {
                herb = (int)i;
                break;
            }
        check(herb >= 0, "(setup) the desert has a gatherable node");
        g.hstate(true, herb).gone = true;
        g.hstate(true, herb).t = 500;
        g.hActive.push_back((1 << 28) | herb);
        g.items.push_back({g.p.pos + V2(20, 0), W_FLINT, 0.f, 0.f});
        const V2 hubPos = g.world.hub[B_DESERT];
        g.revealAround(hubPos);
        const auto nSeen = std::count(g.seen.begin(), g.seen.end(), '1');
        g.travelTo(B_HOME, MAPS[B_DESERT].portals[0].arriveTx, MAPS[B_DESERT].portals[0].arriveTy, false);
        check(g.world.mapId == B_HOME && spotOk(), "the way back leads home to a valid spot");
        check(g.structs.size() == 1 && g.structs[0].tx == homeTx && g.structs[0].ty == homeTy, "the home map still has only its own campfire");
        check(g.items.empty() && g.landmarksFound() == 0 && g.world.landmarks.empty(), "the desert's dropped weapon and landmarks are not in the home map");
        g.travelCd = 0;
        g.travelTo(B_DESERT, MAPS[B_DESERT].entryTx, MAPS[B_DESERT].entryTy, false);
        check(g.structs.size() == 1 && g.structs[0].tx == dTx && g.structs[0].ty == dTy && g.world.solid[dTy][dTx], "returning to the desert restores its campfire (and it blocks the tile)");
        check(g.hstate(true, herb).gone && g.landmarksFound() == 1 && g.world.landmarks[1].found, "...its harvested resource and its discovered landmark");
        check(g.items.size() == 1 && g.items[0].wpn == W_FLINT, "...and the weapon that lay on the ground");
        check(nSeen > 0 && nSeen < (long)MM_W * MM_H && std::count(g.seen.begin(), g.seen.end(), '1') == nSeen, "...and its explored (fog of war) area, which is only the part the hunter has been near");
        check(packTotal() == packNow && g.world.hub[B_DESERT].x == hubPos.x, "nothing was duplicated or lost over two round trips");
        g.travelCd = 0;
        check(!g.travelTo(-1, 5, 5, false) && !g.travelTo(B_COUNT, 5, 5, false) && g.world.mapId == B_DESERT, "an invalid destination is refused");
        g.p.dead = true;
        check(!g.travelTo(B_HOME, 40, 30, false) && g.world.mapId == B_DESERT, "a dead hunter cannot travel");
        g.p.dead = false;
        // ---- a gate trigger: standing in a gate's zone travels, once
        g.travelTo(B_HOME, 40, 30, false);
        g.travelCd = 0;
        {
            const Portal& east = MAPS[B_HOME].portals[0];
            g.p.pos = V2((east.zone.x0 + 0.5f) * TS, 84.5f * TS);
            g.checkPortals(0.f);
            check(g.world.mapId == B_DESERT, "stepping into the east gate leads to the desert");
            g.checkPortals(0.f);
            check(g.world.mapId == B_DESERT && g.travelCd > 0, "arriving does not bounce the hunter straight back");
        }
        // ---- saving in another map and loading
        removeAll();
        g.travelCd = 0;
        g.save();
        g.newGame();
        check(g.world.mapId == B_HOME && !g.mapMem[B_DESERT].visited && g.structs.empty(), "a new game starts in the home map with every map reset");
        const bool ld = g.load();
        check(ld && g.world.mapId == B_DESERT && g.curBiome == B_DESERT && spotOk(), "loading a save made in the desert continues in the desert");
        check(g.structs.size() == 1 && g.structs[0].tx == dTx && g.hstate(true, herb).gone && g.world.landmarks[1].found, "the desert's campfire, harvest and discovered landmark are restored");
        g.travelTo(B_HOME, 40, 30, false);
        check(g.structs.size() == 1 && g.structs[0].tx == homeTx && packTotal() == packNow, "the home map's campfire and the pack survive the save / load too");
        removeAll();
        // ---- a save from before the separate maps (one 240x168 world, home area at tile 76,54)
        {
            g.newGame();
            int hn = -1;
            for (size_t i = 0; i < g.world.nodes.size(); i++)
                if (g.world.nodes[i].kind == RN_HERB && (int)(g.world.nodes[i].x / TS) < COX + CW - 5 && (int)(g.world.nodes[i].x / TS) >= COX && (int)(g.world.nodes[i].y / TS) >= COY && (int)(g.world.nodes[i].y / TS) < COY + CH) {
                    hn = (int)i;
                    break;
                }
            const int htx = (int)(g.world.nodes[hn].x / TS), hty = (int)(g.world.nodes[hn].y / TS);
            std::string t = "version 2\ncoins 40\nup 0 0 0 0\nday 30\nkills 0\nquest 0 0 0\ndisc 10000100001000\n";
            t += std::string("slot ") + itemKey(itemWpn(W_FLINT)) + " 8\n";
            t += "hv n " + S(htx) + " " + S(hty) + " " + S(nodeRes(RN_HERB)) + " 120\n";
            t += "hv n 9 9 0 50\n";  // a resource that was out in a biome: ignored
            t += "struct " + S(PL_CHEST) + " " + S(30 + 76) + " " + S(20 + 54) + "\n";                 // a chest in the home area
            t += "struct " + S(PL_CHEST) + " 200 100\n";                                               // a chest out in the biomes
            t += std::string("cslot 0 ") + itemKey(itemMat(M_WOOD)) + " 5\n";
            t += std::string("cslot 1 ") + itemKey(itemMat(M_STONE)) + " 3\n";
            writeFile(t);
            const bool ok = g.load();
            check(ok && g.world.mapId == B_HOME && g.p.coins == 40, "an old save still loads");
            check(g.structs.size() == 1 && g.structs[0].kind == PL_CHEST && g.structs[0].tx == COX + 30 && g.structs[0].ty == COY + 20 && g.structs[0].inv.count(itemMat(M_WOOD)) == 5, "its home chest moves into the home map with its contents");
            check(g.hstate(true, hn).gone, "its harvested herb is still harvested");
            check(g.p.inv.count(itemPlace(PL_CHEST)) == 1 && g.p.mat(M_STONE) == 3, "a chest that stood out in the biomes comes back to the pack with its contents (nothing is lost)");
            check(g.mapMem[B_DESERT].disc == "1000" && g.mapMem[B_SWAMP].disc == "010" && g.mapMem[B_MOUNTAIN].disc == "1000", "old landmark discoveries are split between the maps");
            removeAll();
        }
    }
}
