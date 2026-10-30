// equipment.cpp - what the hunter wears and keeps at hand: the worn bag (sets the carrying-weight limit), the hotbar (items equipped for
// quick use: weapons, healing, food, buffs, a utility slot), dropping items, and the "can't pick that up" feedback.
// Weights live in items.hpp; Inventory::add refuses what is too heavy or does not fit, so every pickup path is covered by it.
#include <algorithm>
#include "game.hpp"

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
    p.inv.remove(id, 1);                                            // the worn bag is not carried, so it has no weight
    if (p.bag >= 0) p.inv.addIgnoringWeight(itemPack(p.bag), 1);    // the bag it replaces goes back (its slot was just freed)
    p.bag = k;
    p.syncPack();
    msg("WEARING " + item(id).name + " - CARRY UP TO " + S(PACK_CARRY_KG[k]) + " KG", C_GOLD);
    audio.play(S_CLICK);
    return true;
}

bool Game::unequipBag() {
    if (p.bag < 0) return false;
    if (p.inv.slotRoom(itemPack(p.bag)) < 1) {
        msg("NO FREE SLOT TO PUT THE BAG IN", C_RED);
        audio.play(S_ERROR);
        return false;
    }
    p.inv.addIgnoringWeight(itemPack(p.bag), 1);
    p.bag = -1;
    p.syncPack();
    msg("BAG TAKEN OFF - CARRY UP TO " + S(BASE_CARRY_KG) + " KG", C_DIM);
    audio.play(S_CLICK);
    return true;
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
    p.hot[hotSlot] = itemId;
    if (item(itemId).kind == IK_THROWN || item(itemId).kind == IK_LAUNCHER) {
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
    p.hot[hotSlot] = -1;
    p.charging = false;
    fixSel();
    audio.play(S_CLICK);
}

// The selected weapon is always one that is on the hotbar (if the hotbar has any).
void Game::fixSel() {
    if (p.equippedWpn(p.sel)) return;
    for (int i = 0; i < HOT_N; i++)
        if (p.hot[i] >= 0 && item(p.hot[i]).kind != IK_CONSUMABLE && item(p.hot[i]).kind != IK_PLACEABLE) {
            p.sel = item(p.hot[i]).ref;
            return;
        }
}

// A hotbar key: weapons are selected, consumables used, structures placed.
void Game::hotUse(int hotSlot) {
    if (hotSlot < 0 || hotSlot >= HOT_N || p.hot[hotSlot] < 0) return;
    const int id = p.hot[hotSlot];
    const ItemDef& D = item(id);
    if (D.kind == IK_THROWN || D.kind == IK_LAUNCHER) {
        p.sel = D.ref;
        p.charging = false;
        return;
    }
    if (p.inv.count(id) <= 0) {
        msg("NO " + D.name + " LEFT", C_RED);
        audio.play(S_ERROR);
        return;
    }
    if (D.kind == IK_CONSUMABLE) useConsumable(D.ref);
    else if (D.kind == IK_PLACEABLE) placeStructure(D.ref);
}

// Put (up to n of) the stack in a pack slot on the ground in front of the hunter. Nothing is destroyed: walk over it to take it back.
bool Game::dropSlot(int slot, int n) {
    if (slot < 0 || slot >= p.bagCap() || p.inv.slots[slot].empty()) return false;
    const int id = p.inv.slots[slot].item;
    n = std::min(n, p.inv.slots[slot].n);
    if (n <= 0 || p.inv.remove(id, n) != n) return false;
    GroundItem g{};
    g.pos = p.pos + p.aim * 9.f;
    if (world.blocked(g.pos.x, g.pos.y, 3.f)) g.pos = p.pos;
    g.wpn = 0;
    g.angle = 0;
    g.t = 0;
    g.ammo = -1;
    g.item = id;
    g.n = n;
    g.lock = 1.5f;
    items.push_back(g);
    msg("DROPPED " + S(n) + " " + item(id).name, C_DIM);
    audio.play(S_CLICK, 0.5f);
    return true;
}
