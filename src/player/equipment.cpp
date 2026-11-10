// equipment.cpp - what the hunter wears and keeps at hand: the worn bag (sets the carrying-weight limit), the hotbar (items equipped for
// quick use: weapons, healing, food, buffs, a utility slot), dropping items, and the "can't pick that up" feedback.
// Weights live in items.hpp; Inventory::add refuses what is too heavy or does not fit, so every pickup path is covered by it.
#include <algorithm>
#include "core/game.hpp"

// Feedback after something could not be picked up: all 100 slots used, or too heavy for the bag.
void Game::notifyBlocked(int itemId, int n, const Inventory* ref) {
    if (bagWarn > 0) return;
    bagWarn = 1.6f;
    if ((ref ? *ref : p.inv).blockReason(itemId, std::max(1, n)) == 2) {
        ftext(p.pos + V2(0, -28), "TOO HEAVY!", C_RED, 1.2f);
        msg("TOO HEAVY " + kgNum(p.inv.totalWeight()) + "/" + kgNum(p.inv.maxWeight) + " KG - DROP, SELL OR STORE SOMETHING", C_RED);
    } else {
        ftext(p.pos + V2(0, -28), "PACK FULL!", C_RED, 1.2f);
        msg("PACK FULL - ALL " + S(p.bagCap()) + " SLOTS ARE USED", C_RED);
    }
    audio.play(S_ERROR);
}

bool Game::equipBag(int slot) {
    if (slot < 0 || slot >= p.bagCap() || p.inv.slots[slot].empty()) return false;
    const int id = p.inv.slots[slot].item;
    if (item(id).kind != IK_BACKPACK) return false;
    const int k = item(id).ref;
    const int q = p.inv.slots[slot].q;
    p.inv.removeQ(id, q, 1);                                                // the worn bag is not carried, so it has no weight
    if (p.bag >= 0) p.inv.addIgnoringWeight(itemPack(p.bag), 1, p.bagQ);    // the bag it replaces goes back (its slot was just freed)
    p.bag = k;
    p.bagQ = q;
    p.syncPack();
    msg("WEARING " + item(id).name + " - CARRY UP TO " + S(p.carryLimit()) + " KG", C_GOLD);
    audio.play(S_CLICK);
    return true;
}

bool Game::unequipBag() {
    if (p.bag < 0) return false;
    if (p.inv.slotRoom(itemPack(p.bag), p.bagQ) < 1) {
        msg("NO FREE SLOT TO PUT THE BAG IN", C_RED);
        audio.play(S_ERROR);
        return false;
    }
    p.inv.addIgnoringWeight(itemPack(p.bag), 1, p.bagQ);
    p.bag = -1;
    p.bagQ = 0;
    p.syncPack();
    msg("BAG TAKEN OFF - CARRY UP TO " + S(BASE_CARRY_KG) + " KG", C_DIM);
    audio.play(S_CLICK);
    return true;
}

// Farming tools (hoe, watering can, sickle) are equipped like weapons: the tool stays in the pack and sits on a hotbar slot (any slot),
// and p.tool[type] names the one the farm code uses (bestTool() ignores farm tools that are only lying in the pack).
bool Game::ownsTool(int toolId) const {
    if (toolId < 0 || toolId >= TL_COUNT) return false;
    return p.tool[TOOLDEF[toolId].type] == toolId || p.inv.count(itemTool(toolId)) > 0;
}

// Keeps the two views of "equipped" in step: a tool that left the pack (dropped, sold) is no longer equipped, and the hotbar never
// holds a farm tool that is not the equipped one of its type (or one that is no longer in the pack).
void Game::syncFarmTools() {
    if (p.hotSel >= 0 && p.hot[p.hotSel] < 0) {  // the selected slot was emptied
        p.hotSel = -1;
        if (p.hand >= 0) p.hand = -1;
    }
    if (p.hand >= 0 && p.hand < HAND_SEED0 && p.tool[p.hand] < 0) p.hand = -1;  // (the tool in hand was taken off or lost)
    if (p.hand >= HAND_SEED0 && p.hand < HAND_ITEM && p.hotSlotOf(itemMat(cropSeed(p.hand - HAND_SEED0))) < 0) p.hand = -1;  // (the seeds were taken off the hotbar)
    for (int t = 0; t < TT_COUNT; t++) {
        if (!isFarmType(t)) continue;
        if (p.tool[t] >= 0 && p.inv.count(itemTool(p.tool[t])) <= 0) p.tool[t] = -1;
        for (int i = 0; i < HOT_N; i++) {
            const int id = p.hot[i];
            if (id < 0 || item(id).kind != IK_TOOL || TOOLDEF[item(id).ref].type != t) continue;
            if (p.tool[t] != item(id).ref) p.hot[i] = -1;
        }
    }
}

bool Game::equipFarmTool(int slot) {
    if (slot < 0 || slot >= p.bagCap() || p.inv.slots[slot].empty()) return false;
    const int id = p.inv.slots[slot].item;
    if (item(id).kind != IK_TOOL || !isFarmType(TOOLDEF[item(id).ref].type)) {
        msg("THAT IS NOT A FARMING TOOL", C_RED);
        audio.play(S_ERROR);
        return false;
    }
    const int k = item(id).ref, type = TOOLDEF[k].type;
    if (p.hotSlotOf(id) >= 0 && p.tool[type] == k) {
        msg(item(id).name + " IS ALREADY EQUIPPED", C_DIM);
        return false;
    }
    bool room = p.hotSlotOf(id) >= 0;
    for (int i = 0; i < HOT_N && !room; i++) {
        const int h = p.hot[i];
        room = h < 0 || (item(h).kind == IK_TOOL && TOOLDEF[item(h).ref].type == type);
    }
    if (room) return equipHot(id);
    p.tool[type] = k;  // the hotbar is full: the tool is still equipped, it just has no slot
    syncFarmTools();
    msg(item(id).name + " EQUIPPED - HOTBAR FULL, NO SLOT", C_GOLD);
    audio.play(S_CLICK);
    return true;
}

bool Game::unequipFarmTool(int type) {
    if (!isFarmType(type) || p.tool[type] < 0) return false;
    const int id = itemTool(p.tool[type]);
    p.tool[type] = -1;
    syncFarmTools();
    msg(item(id).name + " TAKEN OFF", C_DIM);
    audio.play(S_CLICK);
    return true;
}

void Game::giveFarmTool(int toolId) {
    if (toolId < 0 || toolId >= TL_COUNT || !isFarmType(TOOLDEF[toolId].type)) return;
    const int id = itemTool(toolId);
    if (p.inv.count(id) <= 0) p.inv.addIgnoringWeight(id, 1);
    p.tool[TOOLDEF[toolId].type] = toolId;
    syncFarmTools();
    if (p.hotSlotOf(id) < 0) equipHotQuiet(id);
}

// Put an item on the hotbar. The item stays in the pack (so it still counts for weight); the hotbar only says it is equipped.
bool Game::equipHot(int itemId, int hotSlot) {
    if (!itemValid(itemId) || p.inv.count(itemId) <= 0) return false;
    bool fitsAny = false;
    for (int i = 0; i < HOT_N; i++) fitsAny = fitsAny || hotAccepts(HOT_KIND[i], itemId);
    if (!fitsAny) {
        msg(item(itemId).name + " CANNOT GO ON THE HOTBAR", C_RED);
        audio.play(S_ERROR);
        return false;
    }
    const int cur = p.hotSlotOf(itemId);
    if (hotSlot < 0) {
        if (cur >= 0) {
            msg(item(itemId).name + " IS ALREADY EQUIPPED", C_DIM);
            return false;
        }
        const bool farm = item(itemId).kind == IK_TOOL;
        for (int i = 0; i < HOT_N && hotSlot < 0 && farm; i++)  // a farming tool takes the slot of the tool of its type it replaces
            if (p.hot[i] >= 0 && item(p.hot[i]).kind == IK_TOOL && TOOLDEF[item(p.hot[i]).ref].type == TOOLDEF[item(itemId).ref].type) hotSlot = i;
        for (int i = 0; i < HOT_N && hotSlot < 0; i++)
            if (p.hot[i] < 0 && hotAccepts(HOT_KIND[i], itemId)) hotSlot = i;
        if (hotSlot < 0) {
            msg("ALL THE SLOTS FOR THIS ITEM ARE FULL - UNEQUIP ONE FIRST", C_RED);
            audio.play(S_ERROR);
            return false;
        }
    } else if (hotSlot >= HOT_N || !hotAccepts(HOT_KIND[hotSlot], itemId)) {
        msg(std::string("THAT SLOT TAKES ") + (hotSlot < HOT_N ? HOT_KIND_NAME[HOT_KIND[hotSlot]] : "NOTHING") + " ONLY", C_RED);
        audio.play(S_ERROR);
        return false;
    }
    if (cur >= 0) p.hot[cur] = -1;
    {  // whatever farming tool this slot held is no longer equipped
        const int old = p.hot[hotSlot];
        if (old >= 0 && item(old).kind == IK_TOOL && p.tool[TOOLDEF[item(old).ref].type] == item(old).ref) p.tool[TOOLDEF[item(old).ref].type] = -1;
    }
    p.hot[hotSlot] = itemId;
    if (item(itemId).kind == IK_TOOL) {
        p.tool[TOOLDEF[item(itemId).ref].type] = item(itemId).ref;
        syncFarmTools();
    }
    if (item(itemId).kind == IK_THROWN || item(itemId).kind == IK_LAUNCHER) {
        p.hand = -1;
        p.sel = item(itemId).ref;
        p.charging = false;
    }
    msg("EQUIPPED " + item(itemId).name + " - SLOT " + S(hotSlot + 1), C_GOLD);
    audio.play(S_CLICK);
    return true;
}

bool Game::equipHotQuiet(int itemId) {
    if (!itemValid(itemId) || p.hotSlotOf(itemId) >= 0) return false;
    for (int i = 0; i < HOT_N; i++)
        if (p.hot[i] < 0 && hotAccepts(HOT_KIND[i], itemId)) {
            p.hot[i] = itemId;
            return true;
        }
    return false;
}

void Game::unequipHot(int hotSlot) {
    if (hotSlot < 0 || hotSlot >= HOT_N || p.hot[hotSlot] < 0) return;
    msg("UNEQUIPPED " + item(p.hot[hotSlot]).name, C_DIM);
    {
        const int id = p.hot[hotSlot];
        if (item(id).kind == IK_TOOL && p.tool[TOOLDEF[item(id).ref].type] == item(id).ref) p.tool[TOOLDEF[item(id).ref].type] = -1;
    }
    p.hot[hotSlot] = -1;
    p.charging = false;
    syncFarmTools();
    fixSel();
    audio.play(S_CLICK);
}

// The selected weapon is always one that is on the hotbar (if the hotbar has any).
void Game::fixSel() {
    if (p.equippedWpn(p.sel)) return;
    for (int i = 0; i < HOT_N; i++)
        if (p.hot[i] >= 0 && (item(p.hot[i]).kind == IK_THROWN || item(p.hot[i]).kind == IK_LAUNCHER)) {
            p.sel = item(p.hot[i]).ref;
            return;
        }
}

// What a click does with the item in hand, for the message and the hotbar caption.
static std::string handHint(const Game& g, int id, bool inHome) {
    const ItemDef& D = item(id);
    if (D.kind == IK_THROWN || D.kind == IK_LAUNCHER) return "";
    if (seedCropOf(id) >= 0) return inHome ? " - LEFT CLICK TILLED SOIL" : " - FOR YOUR HOMESTEAD";
    switch (D.kind) {
        case IK_TOOL: return Game::isFarmType(TOOLDEF[D.ref].type) ? (inHome ? " - LEFT CLICK A TILE" : " - FOR YOUR HOMESTEAD") : "";
        case IK_CONSUMABLE: return " - LEFT CLICK TO USE";
        case IK_PLACEABLE: return " - LEFT CLICK TO PLACE";
        case IK_BACKPACK: return " - LEFT CLICK TO WEAR";
        default: return "";
    }
    (void)g;
}

// Take a hotbar item in hand without using it (the wheel and the number keys of non-consumables): a weapon becomes the selected one,
// a farm tool or seeds are held for the soil, anything else is held and used by a left click (hotClick).
void Game::hotSelect(int hotSlot, bool say) {
    if (hotSlot < 0 || hotSlot >= HOT_N || p.hot[hotSlot] < 0) return;
    const int id = p.hot[hotSlot];
    const ItemDef& D = item(id);
    p.hotSel = hotSlot;
    p.charging = false;
    if (D.kind == IK_THROWN || D.kind == IK_LAUNCHER) {
        p.sel = D.ref;
        p.hand = -1;
        return;
    }
    if (seedCropOf(id) >= 0) p.hand = HAND_SEED0 + seedCropOf(id);
    else if (D.kind == IK_TOOL && isFarmType(TOOLDEF[D.ref].type)) {
        p.tool[TOOLDEF[D.ref].type] = D.ref;
        p.hand = TOOLDEF[D.ref].type;
        syncFarmTools();
    } else p.hand = HAND_ITEM;
    if (say) {
        msg("IN HAND: " + D.name + handHint(*this, id, inHomestead()), C_GOLD);
        audio.play(S_CLICK);
    }
}

// A hotbar key: weapons, tools, seeds and other items are taken in hand; food / healing is used and structures are placed at once.
void Game::hotUse(int hotSlot) {
    if (hotSlot < 0 || hotSlot >= HOT_N || p.hot[hotSlot] < 0) return;
    const int id = p.hot[hotSlot];
    const ItemDef& D = item(id);
    if (D.kind == IK_THROWN || D.kind == IK_LAUNCHER) {
        hotSelect(hotSlot, false);
        return;
    }
    if (p.inv.count(id) <= 0) {
        msg("NO " + D.name + " LEFT", C_RED);
        audio.play(S_ERROR);
        return;
    }
    if (D.kind == IK_CONSUMABLE) useConsumable(D.ref);
    else if (D.kind == IK_PLACEABLE) placeStructure(D.ref);
    else hotSelect(hotSlot, true);
}

// Left click with a non-weapon item from the hotbar in hand (hand == HAND_ITEM): use it, place it or wear it.
bool Game::hotClick() {
    if (p.hotSel < 0 || p.hotSel >= HOT_N || p.hot[p.hotSel] < 0) { p.hand = -1; return false; }
    const int id = p.hot[p.hotSel];
    const ItemDef& D = item(id);
    if (p.inv.count(id) <= 0) {
        msg("NO " + D.name + " LEFT", C_RED);
        audio.play(S_ERROR);
        return true;
    }
    if (D.kind == IK_CONSUMABLE) return useConsumable(D.ref);
    if (D.kind == IK_PLACEABLE) return placeStructure(D.ref);
    for (int s = 0; s < p.bagCap(); s++)  // anything else: what the pack's action button would do (wear a bag, explain a material...)
        if (p.inv.slots[s].item == id && !p.inv.slots[s].empty()) return useSlot(s);
    return false;
}

// Put (up to n of) the stack in a pack slot on the ground in front of the hunter. Nothing is destroyed: walk over it to take it back.
bool Game::dropSlot(int slot, int n) {
    if (slot < 0 || slot >= p.bagCap() || p.inv.slots[slot].empty()) return false;
    const int id = p.inv.slots[slot].item;
    n = std::min(n, p.inv.slots[slot].n);
    const int q = p.inv.slots[slot].q;
    if (n <= 0 || p.inv.removeQ(id, q, n) != n) return false;
    GroundItem g{};
    g.pos = p.pos + p.aim * 9.f;
    if (world.blocked(g.pos.x, g.pos.y, 3.f)) g.pos = p.pos;
    g.wpn = 0;
    g.angle = 0;
    g.t = 0;
    g.ammo = -1;
    g.item = id;
    g.n = n;
    g.q = q;
    g.lock = 1.5f;
    items.push_back(g);
    msg("DROPPED " + S(n) + " " + item(id).name, C_DIM);
    audio.play(S_CLICK, 0.5f);
    return true;
}
