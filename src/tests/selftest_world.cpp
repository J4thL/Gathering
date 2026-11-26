// selftest_world.cpp - `--selftest`: gathering, structures, consumables, gear, maps and travel.
#include <set>
#include <tuple>
#include "tests/selftest_ctx.hpp"
#include "world/gates.hpp"

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
            check(harvestable > 0 && bad == 0, "trees on the map border are never harvestable");
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
            const int rArmor1 = recipeFor(RK_ITEM, itemCarry(CG_ARMOR1), M_HIDE), rArmor2 = recipeFor(RK_ITEM, itemCarry(CG_ARMOR2), M_FUR);
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
                if (R.kind != RK_BANDAGE) ok = ok && recipeItem(R) > 0 && recipeItem(R) < ITEM_COUNT;
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
            for (int m : {M_BERRY, M_WHEAT, M_CARROT, M_POTATO, M_CABBAGE, M_CORN, M_TOMATO, M_PUMPKIN, M_STRAWBERRY, M_EGG, M_DUCKEGG, M_MILK, M_WOOL, M_TRUFFLE, M_HAY,
                          M_TROUT, M_BASS, M_CATFISH, M_EEL, M_CHAR, M_PIKE,
                          M_EGGPLANT, M_TURNIP, M_PEPPER, M_CUCUMBER, M_BEET, M_LEEK})
                producible[m] = true;  // grown, raised on the homestead or caught with a rod
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
            check(placeAtCursor(g, PL_CAMPFIRE) && g.structs.size() == 1 && g.p.inv.count(itemPlace(PL_CAMPFIRE)) == 0, "the campfire is placed");
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
            auto wearItem = [&](int id) {
                for (size_t i = 0; i < g.p.inv.slots.size(); i++)
                    if (!g.p.inv.slots[i].empty() && g.p.inv.slots[i].item == id) return g.equipCarried((int)i);
                return false;
            };
            check(g.craftBlock(rArmor2) != "MAKE THE PREVIOUS LEVEL FIRST", "armor has no 'previous level' lock any more");
            check(g.craft(rArmor1) && g.p.inv.count(itemCarry(CG_ARMOR1)) == 1 && g.p.armorLv() == 0, "crafted armor lands in the pack: it only counts once it is worn");
            check(wearItem(itemCarry(CG_ARMOR1)) && g.p.armorLv() == 1 && g.p.inv.count(itemCarry(CG_ARMOR1)) == 0, "wearing it takes it out of the pack");
            check(g.craft(rArmor2) && wearItem(itemCarry(CG_ARMOR2)) && g.p.armorLv() == 2 && g.p.inv.count(itemCarry(CG_ARMOR1)) == 1, "a better piece swaps the old one back into the pack");
            g.p.inv.add(itemMat(M_HIDE), 20);
            g.p.inv.add(itemMat(M_FIBER), 6);
            check(g.craft(rArmor1) && g.craft(rArmor1) && g.p.inv.count(itemCarry(CG_ARMOR1)) == 3, "any piece can be crafted again and again");
            check(g.unequipCarried(EQ_BODY) && g.p.armorLv() == 0 && g.p.inv.count(itemCarry(CG_ARMOR2)) == 1, "armor comes off again (its health bonus with it)");
            check(wearItem(itemCarry(CG_BOOTS1)) == false, "no boots yet: nothing to wear");
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
            check(placeAtCursor(g, PL_CHEST) && g.structs.size() == 2, "a chest is placed");
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
        g.p.inv.clear(); for (int e_ = 0; e_ < EQ_COUNT; e_++) g.p.worn[e_] = -1;
        float dr = 1, rg = 1;
        g.curBiome = B_DESERT;
        g.dayT = 0.5f;
        g.climateMods(dr, rg);
        const float plainHeat = dr;
        g.p.inv.add(itemCarry(CG_DESERTWRAPS), 1); g.migrateWornGear();
        g.climateMods(dr, rg);
        check(plainHeat > 1.f && dr < plainHeat && dr >= 1.f, "desert wraps cut the heat drain while carried");
        g.p.inv.clear(); for (int e_ = 0; e_ < EQ_COUNT; e_++) g.p.worn[e_] = -1;
        g.curBiome = B_SNOW;
        g.p.warmT = 0;
        g.climateMods(dr, rg);
        const float cold = dr;
        g.p.inv.add(itemCarry(CG_WOLFCLOAK), 1); g.migrateWornGear();
        g.climateMods(dr, rg);
        const float cloak = dr;
        g.p.inv.add(itemCarry(CG_PARKA), 1); g.equipCarried([&] { for (int i = 0; i < g.p.bagCap(); i++) if (g.p.inv.slots[i].item == itemCarry(CG_PARKA)) return i; return -1; }());  // a cloak replaces the one worn
        g.climateMods(dr, rg);
        check(cold > cloak && cloak > dr && dr == 1.f, "the wolf cloak halves the cold and the parka removes it");
        g.p.inv.clear(); for (int e_ = 0; e_ < EQ_COUNT; e_++) g.p.worn[e_] = -1;
        g.curBiome = B_MOUNTAIN;
        g.climateMods(dr, rg);
        const float thin = dr;
        g.p.inv.add(itemCarry(CG_HARNESS), 1); g.migrateWornGear();
        g.climateMods(dr, rg);
        check(dr < thin, "the climbing harness eases thin air");
        // snake wraps: only snake bites are softened
        g.p.inv.clear(); for (int e_ = 0; e_ < EQ_COUNT; e_++) g.p.worn[e_] = -1;
        g.p.hp = g.p.maxhp = 200;
        g.hurtPlayer(20, V2(0, 0), A_RATTLER);
        const float plain = 200 - g.p.hp;
        g.p.hp = 200;
        g.p.invuln = 0;
        g.p.inv.add(itemCarry(CG_VENOMWRAPS), 1); g.migrateWornGear();
        g.hurtPlayer(20, V2(0, 0), A_RATTLER);
        const float wrapped = 200 - g.p.hp;
        g.p.hp = 200;
        g.p.invuln = 0;
        g.hurtPlayer(20, V2(0, 0), A_WOLF);
        check(wrapped < plain && 200 - g.p.hp == plain, "venom wraps soften snake bites but not other attacks");
        // gear tiers
        g.p.inv.clear(); for (int e_ = 0; e_ < EQ_COUNT; e_++) g.p.worn[e_] = -1;
        bool tiers = true;
        for (int u = 0; u < 4; u++) tiers = tiers && (u == U_BAG || u == U_TOOL || std::string(gearName(u, UP_MAX[u])).size() > 0);  // bags are items, the tool upgrade is gone
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
        // ---- the home forest's floors come from a density and a moisture field: every floor appears, in believable shares, on every seed
        {
            bool allSeeds = true;
            for (uint32_t sd : {WORLD_SEED, 12345u, 777u, 424242u}) {
                World w;
                w.generate(sd, B_HOME);
                long n[T_LAKE_MID + 1] = {};
                long forest = 0;
                for (int y = 0; y < MH; y++)
                    for (int x = 0; x < MW; x++)
                        if (!(x >= COX && y >= COY && x < COX + CW && y < COY + CH) && !tileLake(w.tile[y][x]) && w.tile[y][x] != T_DIRT) {
                            n[w.tile[y][x]]++;
                            forest++;
                        }
                auto pct = [&](int t) { return 100.0 * n[t] / std::max(1L, forest); };
                printf("  seed %u: grass %.1f%% dark %.1f%% leaf %.1f%% needles %.1f%% moss %.1f%%\n", sd, pct(T_GRASS), pct(T_DARK), pct(T_LEAF),
                       pct(T_NEEDLES), pct(T_MOSS));
                // the meadow stays the biggest single floor; every forest floor is there, and none swallows the forest
                bool ok = pct(T_GRASS) > 25.0 && pct(T_GRASS) < 75.0;
                for (int t : {(int)T_DARK, (int)T_LEAF, (int)T_NEEDLES, (int)T_MOSS}) ok = ok && pct(t) > 1.0 && pct(t) < 30.0;
                allSeeds = allSeeds && ok;
                // trees follow the floor: needles carry mostly pines, leaf litter mostly oaks
                int pineOnNeedles = 0, treesOnNeedles = 0, oakOnLeaf = 0, treesOnLeaf = 0;
                for (const Prop& pr : w.props) {
                    const uint8_t t = w.tileAt(pr.x, pr.y - 1.f);
                    if (!isTreeKind(pr.kind) || pr.x < 8 * TS || pr.y < 8 * TS || pr.x > (MW - 8) * TS || pr.y > (MH - 8) * TS) continue;  // (the edge wall is its own thing)
                    if (t == T_NEEDLES) { treesOnNeedles++; pineOnNeedles += pr.kind == P_PINE; }
                    if (t == T_LEAF) { treesOnLeaf++; oakOnLeaf += pr.kind == P_OAK0 || pr.kind == P_OAK1; }
                }
                if (treesOnNeedles > 20) allSeeds = allSeeds && pineOnNeedles * 10 > treesOnNeedles * 7;
                if (treesOnLeaf > 20) allSeeds = allSeeds && oakOnLeaf * 10 > treesOnLeaf * 8;
            }
            // ---- glades: open clearings in the forest, each with its own feature
            bool gladesOk = true;
            int kindsSeen[GL_COUNT] = {};
            for (uint32_t sd : {WORLD_SEED, 12345u, 777u, 424242u}) {
                World w;
                w.generate(sd, B_HOME);
                if (w.glades.size() < 8) gladesOk = false;
                for (const Glade& gl : w.glades) {
                    kindsSeen[gl.kind]++;
                    const int cx = (int)gl.cx, cy = (int)gl.cy;
                    if (tileLake(w.tile[cy][cx]) || (cx >= COX && cx < COX + CW && cy >= COY && cy < COY + CH)) gladesOk = false;
                    int trees = 0;  // no tree inside the open part of the clearing, except a deliberate centrepiece (one oak)
                    for (const Prop& pr : w.props)
                        if (isTreeKind(pr.kind) && hypotf((pr.x / TS - gl.cx) / gl.rx, ((pr.y - 1.f) / TS - gl.cy) / gl.ry) < 0.6f) trees++;
                    if (trees > (gl.kind == GL_OAK ? 1 : 0)) { gladesOk = false; printf("    glade kind %d at (%.0f,%.0f): %d trees inside\n", gl.kind, gl.cx, gl.cy, trees); }
                    // and the feature exists
                    int feat = 0;
                    for (const Prop& pr : w.props)
                        if ((pr.kind == P_STANDING || pr.kind == P_LOG || pr.kind == P_OAK1) && hypotf(pr.x / TS - gl.cx, pr.y / TS - gl.cy) < gl.rx + 1.f) feat++;
                    for (const ResNode& n : w.nodes)
                        if (hypotf(n.x / TS - gl.cx, n.y / TS - gl.cy) < gl.rx + 1.f) feat++;
                    for (const Decor& d : w.decor)
                        if (d.kind == D_MUSHROOM && hypotf(d.x / TS - gl.cx, d.y / TS - gl.cy) < gl.rx + 1.f) feat++;
                    if (gl.kind != GL_FLOWERS && feat == 0) { gladesOk = false; printf("    glade kind %d at (%.0f,%.0f): no feature\n", gl.kind, gl.cx, gl.cy); }
                }
            }
            int variety = 0;
            for (int k = 0; k < GL_COUNT; k++) variety += kindsSeen[k] > 0;
            check(gladesOk && variety >= 5, "the home forest has 8+ glades per seed: open ground, no trees inside, a feature in each, several kinds");
            // ---- flower fields: wide open meadows carpeted with flowers, with herbs to gather
            bool fieldsOk = true;
            for (uint32_t sd : {WORLD_SEED, 12345u, 777u, 424242u}) {
                World w;
                w.generate(sd, B_HOME);
                if (w.fields.size() < 2) { fieldsOk = false; printf("    seed %u: only %zu flower fields\n", sd, w.fields.size()); }
                for (const FlowerField& f : w.fields) {
                    int trees = 0, flowers = 0, herbs = 0, tiles = 0, offGrass = 0;
                    for (const Prop& pr : w.props)
                        if (isTreeKind(pr.kind) && hypotf((pr.x / TS - f.cx) / f.rx, ((pr.y - 1.f) / TS - f.cy) / f.ry) < 0.7f) trees++;
                    for (const Decor& d : w.decor)
                        if ((d.kind <= D_FLOWER2 || d.kind == D_VIOLET || d.kind == D_POPPY) && hypotf((d.x / TS - f.cx) / f.rx, (d.y / TS - f.cy) / f.ry) < 0.7f) flowers++;
                    for (const ResNode& n : w.nodes)
                        if (n.kind == RN_HERB && hypotf((n.x / TS - f.cx) / f.rx, (n.y / TS - f.cy) / f.ry) < 0.9f) herbs++;
                    for (int y = (int)(f.cy - f.ry); y <= (int)(f.cy + f.ry); y++)
                        for (int x = (int)(f.cx - f.rx); x <= (int)(f.cx + f.rx); x++)
                            if (hypotf((x - f.cx) / f.rx, (y - f.cy) / f.ry) < 0.6f) {
                                tiles++;
                                const uint8_t t = w.tile[y][x];
                                offGrass += t != T_GRASS && t != T_DIRT && t != T_SHALLOW && !tileLake(t) && t != T_MOSS;
                            }
                    printf("    seed %u field at (%.0f,%.0f): %d flowers, %d herbs, %d trees\n", sd, f.cx, f.cy, flowers, herbs, trees);
                    if (trees > 0 || herbs < 3 || offGrass > 0 || flowers < tiles / 2) fieldsOk = false;  // (at least half a flower per tile; the brooks may cross)
                }
            }
            check(fieldsOk, "the home forest has 2+ flower fields per seed: meadow, no trees, carpeted with flowers, herbs to gather");
            // ---- streams, groves, spacing, stumps, logs and dead trees
            bool brooks = true, spacing = true, litter = true;
            for (uint32_t sd : {WORLD_SEED, 12345u, 777u, 424242u}) {
                World w;
                w.generate(sd, B_HOME);
                // brooks: 8-connected runs of shallow water, counted when they touch a lake
                static int comp[MH][MW];
                memset(comp, 0, sizeof comp);
                int streams = 0, longest = 0;
                long shallow = 0;
                for (int y = 0; y < MH; y++)
                    for (int x = 0; x < MW; x++)
                        if (w.tile[y][x] == T_SHALLOW && !comp[y][x]) {
                            std::vector<std::pair<int, int>> st{{x, y}};
                            comp[y][x] = 1;
                            int size = 0;
                            bool lake = false;
                            while (!st.empty()) {
                                auto [cx, cy] = st.back();
                                st.pop_back();
                                size++;
                                for (int dy = -1; dy <= 1; dy++)
                                    for (int dx = -1; dx <= 1; dx++) {
                                        const int nx = cx + dx, ny = cy + dy;
                                        if (nx < 0 || ny < 0 || nx >= MW || ny >= MH) continue;
                                        if (tileLake(w.tile[ny][nx])) lake = true;
                                        if (w.tile[ny][nx] == T_SHALLOW && !comp[ny][nx]) { comp[ny][nx] = 1; st.push_back({nx, ny}); }
                                    }
                            }
                            shallow += size;
                            if (lake && size >= 20) { streams++; longest = std::max(longest, size); }
                        }
                printf("  seed %u: %d brooks reach a lake (longest %d tiles, %ld shallow tiles in all)\n", sd, streams, longest, shallow);
                if (streams < 4) brooks = false;
                // nothing solid grows in a brook
                for (const Prop& pr : w.props)
                    if (w.tileAt(pr.x, pr.y - 1.f) == T_SHALLOW && (isTreeKind(pr.kind) || isRockKind(pr.kind))) brooks = false;
                // open country: trees keep their distance (no two touching), away from the edge wall, the glade oaks and the village
                int touching = 0, stumps = 0, logs = 0, dead = 0;
                for (const Prop& a : w.props) {
                    if (a.kind == P_LOG) logs++;
                    if (a.kind == P_DEADTREE0 || a.kind == P_DEADTREE1) dead++;
                    if (!isTreeKind(a.kind) || a.x < 10 * TS || a.y < 10 * TS || a.x > (MW - 10) * TS || a.y > (MH - 10) * TS) continue;
                    const int tx = (int)(a.x / TS), ty = (int)((a.y - 1.f) / TS);
                    if (tx >= COX - 2 && tx < COX + CW + 2 && ty >= COY - 2 && ty < COY + CH + 2) continue;
                    if (w.homeDens[(size_t)ty * MW + tx] >= 0.5f || w.tile[ty][tx] != T_GRASS) continue;
                    bool glade = false;
                    for (const Glade& gl : w.glades)
                        if (hypotf((tx - gl.cx) / gl.rx, (ty - gl.cy) / gl.ry) < 1.8f) glade = true;
                    if (glade) continue;
                    for (const Prop& b : w.props)
                        if (&a != &b && isTreeKind(b.kind) && hypotf(a.x - b.x, a.y - b.y) < 20.f) { touching++; break; }
                }
                for (const Decor& d : w.decor) stumps += d.kind == D_STUMP;
                printf("    meadow trees touching: %d, stumps %d, logs %d, dead trees %d\n", touching, stumps, logs, dead);
                if (touching > 0) spacing = false;
                if (stumps < 40 || logs < 15 || dead < 15) litter = false;
            }
            check(brooks, "streams: several brooks per seed run to a lake, and nothing solid grows in them");
            check(spacing, "trees in open country keep their distance from each other");
            check(litter, "the forest has old stumps, fallen logs and dead trees");
            // ---- smoothing (#6): far fewer specks of one floor lost inside another
            // ---- independent layers (#8): leaving a stream or a glade out changes nothing it does not touch
            bool smooth = true, independent = true;
            for (uint32_t sd : {WORLD_SEED, 12345u, 777u}) {
                World full, rough, noStreams, noGlades, noFields;
                full.generate(sd, B_HOME);
                rough.genOff = GEN_DEFAULT | GEN_NO_SMOOTH;
                rough.generate(sd, B_HOME);
                noStreams.genOff = GEN_DEFAULT | GEN_NO_STREAMS;
                noStreams.generate(sd, B_HOME);
                noGlades.genOff = GEN_DEFAULT | GEN_NO_GLADES;
                noGlades.generate(sd, B_HOME);
                noFields.genOff = GEN_DEFAULT | GEN_NO_FIELDS;
                noFields.generate(sd, B_HOME);
                auto isFloor = [](uint8_t t) { return t == T_GRASS || t == T_DARK || t == T_LEAF || t == T_MOSS || t == T_NEEDLES || t == T_ROOTS; };
                auto specks = [&](const World& w) {  // floor tiles with at most 1 of their 8 neighbours the same floor (isolated specks and the tips of one-tile spurs), away from water, glades and the village
                    int n = 0;
                    for (int y = 6; y < MH - 6; y++)
                        for (int x = 6; x < MW - 6; x++) {
                            const uint8_t t = w.tile[y][x];
                            if (!isFloor(t) || (x >= COX - 2 && x < COX + CW + 2 && y >= COY - 2 && y < COY + CH + 2)) continue;
                            int same = 0;
                            bool wet = false;
                            for (int dy = -2; dy <= 2; dy++)
                                for (int dx = -2; dx <= 2; dx++) {
                                    const uint8_t o = w.tile[y + dy][x + dx];
                                    if (tileLake(o) || o == T_SHALLOW) wet = true;
                                    if (abs(dx) <= 1 && abs(dy) <= 1 && (dx || dy)) same += o == t;
                                }
                            bool glade = false;
                            for (const Glade& gl : w.glades) if (hypotf((x - gl.cx) / gl.rx, (y - gl.cy) / gl.ry) < 1.6f) glade = true;
                            n += !wet && !glade && same <= 1;
                        }
                    return n;
                };
                const int sRough = specks(rough), sFull = specks(full);
                printf("  seed %u: floor specks %d before smoothing, %d after\n", sd, sRough, sFull);
                if (!(sFull * 2 <= sRough)) smooth = false;
                // trees that are not near the layer that was left out must be the very same trees
                auto treeSet = [&](const World& w) {
                    std::set<std::tuple<int, int, int>> t;
                    for (const Prop& pr : w.props) if (isTreeKind(pr.kind)) t.insert({pr.kind, (int)lroundf(pr.x), (int)lroundf(pr.y)});
                    return t;
                };
                auto compare = [&](const World& a, const World& b, auto near, const char* what) {
                    const auto tb = treeSet(b);
                    int checked = 0, missing = 0;
                    for (const Prop& pr : a.props) {
                        if (!isTreeKind(pr.kind)) continue;
                        const int tx = (int)(pr.x / TS), ty = (int)((pr.y - 1.f) / TS);
                        if (near(tx, ty)) continue;
                        checked++;
                        missing += !tb.count({pr.kind, (int)lroundf(pr.x), (int)lroundf(pr.y)});
                    }
                    printf("    %s: %d trees away from it, %d differ\n", what, checked, missing);
                    return checked > 200 && missing * 50 <= checked;  // at most 2% (a chain of spacing choices can ripple a little)
                };
                independent = independent && compare(full, noStreams, [&](int x, int y) {
                    for (int dy = -4; dy <= 4; dy++)
                        for (int dx = -4; dx <= 4; dx++) {
                            const int nx = x + dx, ny = y + dy;
                            if (nx >= 0 && ny >= 0 && nx < MW && ny < MH && full.tile[ny][nx] == T_SHALLOW) return true;
                        }
                    return false; }, "without streams");
                independent = independent && compare(full, noGlades, [&](int x, int y) {
                    for (const Glade& gl : full.glades) if (hypotf((x - gl.cx) / gl.rx, (y - gl.cy) / gl.ry) < 2.2f) return true;
                    return false; }, "without glades");
                independent = independent && compare(full, noFields, [&](int x, int y) {
                    for (const FlowerField& f : full.fields) if (hypotf((x - f.cx) / f.rx, (y - f.cy) / f.ry) < 1.8f) return true;
                    return false; }, "without flower fields");
                independent = independent && noFields.fields.empty() && !full.fields.empty();
                // and the layers really are there when switched on
                independent = independent && noStreams.tile[1][1] != T_SHALLOW && noGlades.glades.empty() && !full.glades.empty();
            }
            check(smooth, "smoothing removes at least half of the floor specks");
            check(independent, "leaving out streams, glades or flower fields leaves every tree away from them exactly where it was");
            check(allSeeds, "the home forest has meadow, dark grass, leaf litter, needles and moss in sensible shares, and trees match their floor");
        }
        // ---- snowfield plateaus: raised ground with a rock face on its south side, ramps up, a walking-stopping lip on the other sides
        {
            bool shapes = true, ramps = true, reach = true, lip = true, same = true, off = true, sizes = true, tops = true, edgesClear = true;
            long totalPlat = 0;
            for (uint32_t sd : {WORLD_SEED, 12345u, 777u}) {
                World w, w2, flat;
                w.genOff = GEN_DEFAULT | GEN_V2_SNOW | GEN_V2_PLATEAU;
                w.generate(sd, B_SNOW);
                w2.genOff = w.genOff;
                w2.generate(sd, B_SNOW);
                flat.genOff = (GEN_DEFAULT | GEN_V2_SNOW) & ~GEN_V2_PLATEAU;
                flat.generate(sd, B_SNOW);
                long nPlat = 0, nRamp = 0;
                for (int y = 0; y < MH; y++)
                    for (int x = 0; x < MW; x++) {
                        nPlat += w.plat[y][x] == 1;
                        nRamp += w.plat[y][x] == 2;
                        same = same && w.tile[y][x] == w2.tile[y][x] && w.plat[y][x] == w2.plat[y][x] && w.edge[y][x] == w2.edge[y][x];
                        off = off && flat.plat[y][x] == 0 && flat.edge[y][x] == 0;
                        // the tile under a plateau's bottom edge is rock face (two tiles), unless it is a ramp
                        if (w.plat[y][x] == 1 && y + 2 < MH && w.plat[y + 1][x] != 1)
                            shapes = shapes && (w.plat[y + 1][x] == 2 || w.tile[y + 1][x] == T_CLIFF) && (w.plat[y + 2][x] == 2 || w.tile[y + 2][x] == T_CLIFF);
                    }
                {   // the sizes vary (a massif of 200+ tiles and a small mesa on every seed), and the tops carry stones, trees, cairns and frostbloom
                    std::vector<int> lab((size_t)MW * MH, 0);
                    int big = 0, small = 0;
                    for (int y = 0; y < MH; y++)
                        for (int x = 0; x < MW; x++) {
                            if (w.plat[y][x] != 1 || lab[(size_t)y * MW + x]) continue;
                            std::vector<int> st{y * MW + x};
                            lab[(size_t)y * MW + x] = 1;
                            int a = 0;
                            while (!st.empty()) {
                                const int c = st.back();
                                st.pop_back();
                                a++;
                                const int cx = c % MW, cy = c / MW, d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                                for (auto& o : d4) {
                                    const int nx = cx + o[0], ny = cy + o[1];
                                    if (nx >= 0 && ny >= 0 && nx < MW && ny < MH && w.plat[ny][nx] == 1 && !lab[(size_t)ny * MW + nx]) lab[(size_t)ny * MW + nx] = 1, st.push_back(ny * MW + nx);
                                }
                            }
                            big += a >= 200;
                            small += a < 100;
                        }
                    sizes = sizes && big >= 1 && small >= 3;
                    int onTop = 0, cairns = 0, bloom = 0;
                    for (const Prop& p : w.props) {
                        const int tx = (int)(p.x / TS), ty = (int)((p.y - 1.f) / TS);
                        if (tx < 0 || ty < 0 || tx >= MW || ty >= MH || w.plat[ty][tx] != 1) continue;
                        onTop++;
                        cairns += p.kind == P_CAIRN;
                        // nothing stands on the lip row: every edge tile of a top is free
                        edgesClear = edgesClear && w.edge[ty][tx] == 0;
                    }
                    for (const ResNode& n : w.nodes) {
                        const int tx = (int)(n.x / TS), ty = (int)(n.y / TS);
                        bloom += tx >= 0 && ty >= 0 && tx < MW && ty < MH && w.plat[ty][tx] == 1;
                    }
                    tops = tops && onTop >= 8 && cairns >= 1 && bloom >= 2;
                }
                totalPlat += nPlat;
                shapes = shapes && nPlat > 0 && nPlat < MW * MH / 3;
                ramps = ramps && nRamp >= 2;
                // every plateau can be walked onto from the hub: flood on 8px cells with the real collision test (as the walk-ability repair does)
                const int gw = MW * TS / 8, gh = MH * TS / 8;
                std::vector<char> seen((size_t)gw * gh, 0);
                std::vector<int> q;
                const int sx = (int)(w.hub[B_SNOW].x / 8), sy = (int)(w.hub[B_SNOW].y / 8);
                auto pass = [&](int cx, int cy) { return cx >= 0 && cy >= 0 && cx < gw && cy < gh && !w.blocked(cx * 8 + 4.f, cy * 8 + 4.f, 4.f); };
                if (pass(sx, sy)) { q.push_back(sy * gw + sx); seen[sy * gw + sx] = 1; }
                for (size_t h = 0; h < q.size(); h++) {
                    const int cx = q[h] % gw, cy = q[h] / gw;
                    const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (auto& o : d4) {
                        const int nx = cx + o[0], ny = cy + o[1];
                        if (pass(nx, ny) && !seen[ny * gw + nx]) { seen[ny * gw + nx] = 1; q.push_back(ny * gw + nx); }
                    }
                }
                long plateauCells = 0, plateauReached = 0;
                for (int cy = 0; cy < gh; cy++)
                    for (int cx = 0; cx < gw; cx++) {
                        const int tx = cx * 8 / TS, ty = cy * 8 / TS;
                        if (w.plat[ty][tx] != 1 || !pass(cx, cy)) continue;
                        plateauCells++;
                        plateauReached += seen[cy * gw + cx];
                    }
                reach = reach && plateauCells > 0 && plateauReached * 100 >= plateauCells * 98;
                // the lip: a free plateau tile whose east side looks onto plain ground stops the hunter a few pixels short of that side, and does not stop him in the middle
                bool found = false;
                for (int y = 4; y < MH - 4 && !found; y++)
                    for (int x = 4; x < MW - 4 && !found; x++)
                        if (w.plat[y][x] == 1 && (w.edge[y][x] & 2) && !w.solid[y][x] && !w.blocked(x * TS + TS / 2.f, y * TS + TS / 2.f, 4.f)) {
                            found = true;
                            lip = lip && w.blocked(x * TS + TS - 1.f, y * TS + TS / 2.f, 4.f) && !w.blocked(x * TS + TS - EDGE_W - 4.f - 0.5f, y * TS + TS / 2.f, 4.f);
                        }
                lip = lip && found;
            }
            check(shapes && totalPlat > 0, "the snowfield has plateaus on every seed, each with a two-tile rock face (or a ramp) along its south edge, and none covers a third of the map");
            check(ramps, "every seed has ramps up onto the plateaus");
            check(reach, "every plateau can be reached from the hub by walking (through the ramps)");
            check(lip, "the lip along a plateau's outer edge stops the hunter just short of it, and the middle of the plateau is free");
            check(sizes, "plateau sizes vary: every seed has at least one massif of 200+ tiles and several small mesas under 100");
            check(tops, "plateau tops carry stones, lone trees, a cairn and frostbloom on every seed");
            check(edgesClear, "nothing stands on a plateau's edge tiles (the lip row stays free to walk)");
            check(same, "plateaus build identically for the same seed");
            check(off, "with plateaus switched off the snowfield has none");
        }
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
        check(g.world.mapId == B_HOME && placeAtCursor(g, PL_CAMPFIRE) && g.structs.size() == 1, "(setup) a campfire is placed in the home map");
        const int homeTx = g.structs[0].tx, homeTy = g.structs[0].ty;
        g.p.inv.add(itemPlace(PL_CAMPFIRE), 0);
        const long packMid = packTotal();
        check(packMid <= packBefore, "(setup) placing a structure never adds items to the pack");
        check(g.travelTo(B_DESERT, MAPS[B_DESERT].entryTx, MAPS[B_DESERT].entryTy, false) && g.world.mapId == B_DESERT && g.curBiome == B_DESERT, "travelling loads the desert map");
        check(spotOk(), "the hunter arrives on a free tile inside the map");
        check(packTotal() == packMid && g.p.coins == 500 && (int)g.p.hp == 55 && g.p.sk.s[SK_HUNTING].xp == 300 && g.questIdx == 2 && g.dayT == 77.f, "pack, coins, health, skills, quest and time are unchanged by a trip");
        check(g.structs.empty(), "the home campfire is not in the desert (maps do not mix)");
        g.travelCd = 0;
        check(placeAtCursor(g, PL_CAMPFIRE) && g.structs.size() == 1, "(setup) a campfire is placed in the desert");
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
        for (int b = 0; b < B_COUNT; b++) g.gateOpen[b] = true;  // (the gates are sealed until the offering is made; see the gates test)
        g.state = ST_PLAY;
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
    printf("abandoned camps and checkpoints\n");
    {
        // ---- the generator: three or four camps on the desert, swamp and mountain, every campsite of the snowfield (three to six), none found at the
        // start, each with one fire and a blocked tile (and, outside the snowfield, one fallen tent)
        bool counts = true, cold = true, oneFire = true, blockedOk = true, same = true, others = true, warmOk = true;
        int standing = 0, fallen = 0;  // the tents of the other biomes' camps: still standing, come down
        for (int m : {B_DESERT, B_SWAMP, B_MOUNTAIN, B_SNOW}) {
            World a, b;
            a.generate(WORLD_SEED, m);
            b.generate(WORLD_SEED, m);
            counts = counts && a.camps.size() >= 3 && a.camps.size() <= (m == B_SNOW ? 6u : 4u);
            same = same && a.camps.size() == b.camps.size();
            for (size_t i = 0; i < a.camps.size(); i++) {
                const Camp& c = a.camps[i];
                cold = cold && !c.lit;
                blockedOk = blockedOk && a.solidTile((int)(c.x / TS), (int)(c.y / TS));
                int n = 0, tents = 0;
                for (const Prop& pr : a.props) {
                    const bool here = fabsf(pr.x - c.x) < 1.f && fabsf(pr.y - c.y) < 1.f;
                    n += here && ((pr.kind == P_CAMPFIRE && (int)pr.ph == (int)i) || (pr.kind == P_SNOWCAMPFIRE && c.warm));
                    tents += pr.kind == P_CAMPTENT && fabsf(pr.x - (c.x - 40.f)) < 1.f && fabsf(pr.y - (c.y - 14.f)) < 1.f;
                    if (pr.kind == P_CAMPTENT) (pr.ph > 0.5f ? standing : fallen)++;
                }
                oneFire = oneFire && n == 1 && (m == B_SNOW || tents == 1);
                warmOk = warmOk && (m == B_SNOW || !c.warm);
            }
        }
        for (int m : {B_HOME}) {
            World a;
            a.generate(WORLD_SEED, m);
            others = others && a.camps.empty();
        }
        check(counts && same, "the desert, swamp and mountain get three or four camps and the snowfield three to six, the same ones every time");
        check(cold && oneFire && blockedOk && warmOk, "every camp starts unfound, with one fire (blocking its tile) and, outside the snowfield, one fallen tent");
        check(others, "the home forest has no camps");
        check(standing > 0 && fallen > 0, "the camps of the other biomes have both the standing tent and the fallen one");
        // ---- lighting and the checkpoint
        g.newGame();
        g.state = ST_PLAY;
        g.helpT = 0;
        g.travelTo(B_DESERT, MAPS[B_DESERT].entryTx, MAPS[B_DESERT].entryTy, false);
        g.travelCd = 0;
        check(g.world.camps.size() >= 3, "the desert has its camps in the game world");
        if (g.world.camps.size() >= 3) {
            const Camp c0 = g.world.camps[0];
            g.p.pos = V2(c0.x + 90, c0.y + 90);
            g.updateCamps();
            check(!g.world.camps[0].lit, "a camp stays cold while the hunter is far from its fire");
            const V2 entrance = g.world.spawn;
            check(dist(g.respawnPoint(), entrance) < 1.f, "with no camp lit, a knocked-out hunter wakes at the map's entrance");
            g.p.pos = V2(c0.x, c0.y + 30);
            const int64_t xp0 = g.p.sk.s[SK_SURVIVAL].xp;
            g.updateCamps();
            check(g.world.camps[0].lit, "walking up to a camp's fire lights it");
            check(g.p.sk.s[SK_SURVIVAL].xp > xp0, "lighting a camp pays some survival experience");
            const int64_t xp1 = g.p.sk.s[SK_SURVIVAL].xp;
            g.updateCamps();
            check(g.p.sk.s[SK_SURVIVAL].xp == xp1, "a camp pays only once");
            check(g.world.camps[1].lit == false, "the other camps stay cold");
            // knocked out far away: wakes beside the lit camp
            g.p.pos = V2(c0.x + 300, c0.y + 200);
            g.p.hp = 1;
            g.hurtPlayer(50, g.p.pos + V2(10, 0));
            check(g.p.dead, "the hunter is knocked out");
            for (int i = 0; i < 400 && g.p.dead; i++) g.update(0.05f);
            check(!g.p.dead && dist(g.p.pos, V2(c0.x, c0.y + 22.f)) < 6.f, "a knocked-out hunter wakes beside the lit camp");
            check(!g.world.blocked(g.p.pos.x, g.p.pos.y, 5.f), "and does not wake inside anything");
            // the lit camps are saved and come back
            g.save();
            g.world.camps[0].lit = false;
            const bool ld = g.load();
            check(ld && g.world.mapId == B_DESERT && g.world.camps.size() >= 3 && g.world.camps[0].lit && !g.world.camps[1].lit, "a lit camp is still lit after saving and loading");
            // another map keeps its own camps: leave and come back
            g.travelTo(B_HOME, MAPS[B_DESERT].portals[0].arriveTx, MAPS[B_DESERT].portals[0].arriveTy, false);
            g.travelCd = 0;
            check(g.world.camps.empty(), "the home map has no camps");
            g.travelTo(B_DESERT, MAPS[B_DESERT].entryTx, MAPS[B_DESERT].entryTy, false);
            g.travelCd = 0;
            check(g.world.camps.size() >= 3 && g.world.camps[0].lit && !g.world.camps[1].lit, "a camp stays lit when the hunter leaves the map and comes back");
        }
        // ---- the snowfield: a camp is found by walking up to it (a warm one has nothing to light), and the hunter wakes beside it, never inside a log
        g.travelTo(B_SNOW, MAPS[B_SNOW].entryTx, MAPS[B_SNOW].entryTy, false);
        g.travelCd = 0;
        check(g.world.camps.size() >= 3, "the snowfield has its camps in the game world");
        if (g.world.camps.size() >= 3) {
            bool anyWarm = false, anyCold = false;
            for (const Camp& c : g.world.camps) (c.warm ? anyWarm : anyCold) = true;
            check(anyWarm || anyCold, "the snowfield's camps are warm (a burning fire) or cold (an abandoned camp)");
            for (size_t ci = 0; ci < g.world.camps.size(); ci++) {
                const Camp c = g.world.camps[ci];
                g.p.pos = V2(c.x, c.y + 30);
                g.updateCamps();
                check(g.world.camps[ci].lit, "walking up to a snowfield camp finds it");
                g.p.pos = V2(c.x + 300, c.y + 200);
                const V2 wake = g.respawnPoint();
                check(!g.world.blocked(wake.x, wake.y, 5.f) && dist(wake, V2(c.x, c.y)) < 40.f, "and a hunter knocked out far away wakes beside it, never inside a log or a tent");
            }
        }
        // ---- the snowfield's leafy birches: about one birch in four, always a birch, the same trees every time
        {
            World a, b;
            a.generate(WORLD_SEED, B_SNOW);
            b.generate(WORLD_SEED, B_SNOW);
            int birches = 0, leafy = 0, leafyNotBirch = 0, differs = 0;
            for (size_t i = 0; i < a.props.size(); i++) {
                const Prop& pr = a.props[i];
                if (pr.kind != P_BARETREE) continue;
                birches += snowBirchAt(pr.x, pr.y);
                leafy += snowBirchLeafy(pr.x, pr.y);
                leafyNotBirch += snowBirchLeafy(pr.x, pr.y) && !snowBirchAt(pr.x, pr.y);
                differs += i >= b.props.size() || snowBirchLeafy(pr.x, pr.y) != snowBirchLeafy(b.props[i].x, b.props[i].y);
            }
            check(birches >= 20 && leafy * 100 >= birches * 15 && leafy * 100 <= birches * 35, "about one birch in four of the snowfield is a leafy one");
            check(leafyNotBirch == 0 && differs == 0, "a leafy birch is always a birch, and they are the same ones every time");
        }
        removeAll();
        g.loadMapWorld(B_HOME);  // the tests after this one expect the home map
    }
}
