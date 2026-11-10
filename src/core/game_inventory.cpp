// game_inventory.cpp - pack screen actions, consumables, the shop economy and quests.
#include <cstdio>
#include <fstream>
#include <sstream>
#include "core/game.hpp"
#include "core/logic_util.hpp"

// ================================================================= inventory screen

// Eat / drink / apply one consumable (used from the pack and by the Q quick-heal key).
bool Game::useConsumable(int c) {
    const int id = itemCons(c);
    if (p.inv.count(id) <= 0) return false;
    auto heal = [&](int amount) {
        if (p.hp >= p.maxhp) {
            msg("YOU ARE AT FULL HEALTH", C_DIM);
            audio.play(S_ERROR);
            return false;
        }
        amount = (int)lroundf(amount * (1.f + skillBonus(SK_SURVIVAL)));
        p.hp = std::min(p.maxhp, p.hp + amount);
        ftext(p.pos + V2(0, -24), "+" + S(amount) + " HP", C_GREEN, 1.1f);
        return true;
    };
    switch (c) {
        case C_BANDAGE: if (!heal(BANDAGE_HEAL)) return false; break;
        case C_SALVE: if (!heal(30)) return false; break;
        case C_TONIC: if (!heal(90)) return false; break;
        case C_COOKED:
            if (!heal(35)) return false;
            p.stamina = 100.f;
            p.exhausted = false;
            break;
        case C_BREW:
            if (p.warmT > 60.f) {
                msg("YOU ARE ALREADY WARM", C_DIM);
                audio.play(S_ERROR);
                return false;
            }
            p.warmT = 180.f;
            ftext(p.pos + V2(0, -24), "WARM!", C_ORANGE, 1.2f);
            break;
        case C_CACTUSWATER:
            p.stamina = 100.f;
            p.exhausted = false;
            p.coolT = 120.f;
            ftext(p.pos + V2(0, -24), "REFRESHED!", C_BLUE, 1.2f);
            break;
        case C_POULTICE: if (!heal(60)) return false; break;
        case C_MEDICINE: if (!heal(120)) return false; break;
        case C_JERKY:
            if (!heal(20)) return false;
            p.stamina = 100.f;
            p.exhausted = false;
            break;
        case C_STEW:
            if (!heal(85)) return false;
            p.stamina = 100.f;
            p.exhausted = false;
            break;
        case C_VIGOR:
            if (p.hp >= p.maxhp && p.swiftT > 30.f) {
                msg("YOU ARE ALREADY INVIGORATED", C_DIM);
                audio.play(S_ERROR);
                return false;
            }
            p.hp = std::min(p.maxhp, p.hp + 55.f);
            p.stamina = 100.f;
            p.exhausted = false;
            p.swiftT = 90.f;
            ftext(p.pos + V2(0, -24), "INVIGORATED!", C_GREEN, 1.2f);
            break;
        case C_NIGHTSIGHT:
            if (p.nightT > 120.f) {
                msg("YOUR EYES ARE ALREADY SHARP", C_DIM);
                audio.play(S_ERROR);
                return false;
            }
            p.nightT = 300.f;
            ftext(p.pos + V2(0, -24), "NIGHTSIGHT!", C_BLUE, 1.2f);
            break;
        default: return false;
    }
    p.inv.remove(id, 1);
    p.useT = hunterClip(HS_USE).total();  // plays the use animation
    audio.play(S_PICKUP);
    grantXp(SK_SURVIVAL, 2);  // only reached when the item really took effect (refusals return earlier)
    return true;
}

// Equip a weapon or use a consumable from the pack. Returns true when something happened.
bool Game::useSlot(int slot) {
    if (slot < 0 || slot >= p.bagCap() || p.inv.slots[slot].empty()) return false;
    const int id = p.inv.slots[slot].item;
    const ItemDef& D = item(id);
    switch (D.kind) {
        case IK_THROWN:
        case IK_LAUNCHER:  // equip it on the hotbar (or, if it is already there, take it in hand)
            if (p.equippedWpn(D.ref)) {
                p.sel = D.ref;
                p.charging = false;
                msg("IN HAND: " + D.name, C_GOLD);
                audio.play(S_CLICK);
                return true;
            }
            return equipHot(id);
        case IK_BACKPACK: return equipBag(slot);
        case IK_AMMO:
            for (int w = 0; w < W_COUNT; w++)  // ammunition equips the launcher that fires it
                if (WPN[w].ammo == D.ref && p.wpn(w) > 0) {
                    if (p.equippedWpn(w)) {
                        p.sel = w;
                        msg("IN HAND: " + std::string(WPN[w].name), C_GOLD);
                        audio.play(S_CLICK);
                        return true;
                    }
                    return equipHot(itemWpn(w));
                }
            msg("YOU HAVE NOTHING THAT FIRES " + D.name + "S", C_RED);
            audio.play(S_ERROR);
            return false;
        case IK_CONSUMABLE: return useConsumable(D.ref);
        case IK_TOOL:
            if (isFarmType(TOOLDEF[D.ref].type)) return equipFarmTool(slot);
            msg("TOOLS WORK AUTOMATICALLY - THE BEST ONE IN YOUR PACK IS USED", C_DIM);
            return false;
        case IK_PLACEABLE:
            if (placeStructure(D.ref)) {
                state = ST_PLAY;  // close the pack so the new structure is visible
                return true;
            }
            return false;
        default:
            if (seedCropOf(id) >= 0) {  // seeds are equipped on the hotbar and sown with a click on tilled soil
                if (p.hotSlotOf(id) >= 0) { hotSelect(p.hotSlotOf(id)); return true; }
                return equipHot(id);
            }
            if (D.kind == IK_MATERIAL) { msg(D.name + " - SELL IT AT THE SHOP", C_DIM); return false; }
            msg(D.name + " - SELL IT AT THE SHOP", C_DIM);
            return false;
    }
}

void Game::updateInventory(float dt) {
    updateFx(dt);  // time stands still while the pack is open
    if (in.escPressed || in.invToggle) {
        state = ST_PLAY;
        save();
        audio.play(S_CLICK);
        return;
    }
    if (in.tabPressed) invTab = (invTab + 1) % 3;
    // cursor: arrow keys / WASD, one step per key press
    const bool now[4] = {in.up, in.down, in.left, in.right};
    const int cap = p.bagCap();
    if (invTab == 0) {
        int dx = (now[3] && !navPrev[3]) - (now[2] && !navPrev[2]), dy = (now[1] && !navPrev[1]) - (now[0] && !navPrev[0]);
        if (dx || dy) {
            int c = invSel % INV_COLS + dx, r = invSel / INV_COLS + dy;
            int rows = (cap + INV_COLS - 1) / INV_COLS;
            c = std::max(0, std::min(INV_COLS - 1, c));
            r = std::max(0, std::min(rows - 1, r));
            invSel = std::min(cap - 1, r * INV_COLS + c);
        }
        if (in.enterPressed) useSlot(invSel);
    }
    if (invTab == 1) {  // crafting list: arrows and the wheel scroll it
        craftScroll += (now[1] && !navPrev[1]) - (now[0] && !navPrev[0]);
        craftScroll -= in.wheel;
    }
    for (int i = 0; i < 4; i++) navPrev[i] = now[i];
}

// ================================================================= economy
bool Game::canAfford(int coins, int m1, int n1, int m2, int n2) const {
    if (p.coins < coins) return false;
    if (m1 >= 0 && p.mat(m1) < n1) return false;
    if (m2 >= 0 && p.mat(m2) < n2) return false;
    return true;
}
void Game::spend(int coins, int m1, int n1, int m2, int n2) {
    p.coins -= coins;
    if (m1 >= 0) p.inv.remove(itemMat(m1), n1);
    if (m2 >= 0) p.inv.remove(itemMat(m2), n2);
}
bool Game::buyUpgrade(int u) {
    if (u == U_BAG) return false;  // bags are crafted and worn, not bought
    int lv = p.up[u];
    if (lv >= 2) return false;
    const UpLevel& L = UPS[u].lv[lv];
    if (!canAfford(L.coins, L.m1, L.n1, L.m2, L.n2)) {
        audio.play(S_ERROR);
        return false;
    }
    spend(L.coins, L.m1, L.n1, L.m2, L.n2);
    p.up[u]++;
    audio.play(S_CRAFT);
    msg(std::string("UPGRADED: ") + L.name, C_GOLD);
    save();
    return true;
}
void Game::sellMat(int m, int n) {
    n = std::min(n, p.mat(m));
    if (n <= 0) return;
    p.inv.remove(itemMat(m), n);
    p.coins += n * MAT_PRICE[m];
    sold += n * MAT_PRICE[m];
    audio.play(S_COIN);
    questEvent(Q_EARN, 0, n * MAT_PRICE[m]);
}

// ================================================================= quests
bool Game::nearQuestGiver() const { return dist(p.pos, world.questGiver) < 34; }

// Re-read the current quest from the data table (call whenever questIdx changes).
void Game::refreshQuest() {
    questIdx = std::max(0, questIdx);
    quest = questAt(questIdx);
    questProg = questActive ? std::min(std::max(0, questProg), quest.goal) : 0;
}

// Something happened in the world (a kill, gathered material, a craft, coins earned from selling).
// If it matches the active quest, count it.
void Game::questEvent(int type, int id, int n) {
    if (!questActive || questReady() || quest.type != type) return;
    if (quest.target >= 0 && quest.target != id) return;
    questProg = std::min(quest.goal, questProg + n);
    if (questReady()) {
        msg("QUEST DONE: " + quest.title + " - SEE THE ELDER", C_GREEN);
        ftext(p.pos + V2(0, -30), "QUEST DONE!", C_GREEN, 1.6f);
        audio.play(S_PICKUP);
    }
}

void Game::acceptQuest() {
    if (questActive) return;
    questActive = true;
    questProg = 0;
    msg("QUEST ACCEPTED: " + quest.title, C_GOLD);
    audio.play(S_PICKUP);
    save();
}

void Game::turnInQuest() {
    if (!questReady()) return;
    p.coins += quest.coins;
    std::string reward = S(quest.coins) + " COINS";
    if (quest.mat >= 0 && quest.matN > 0) {
        // materials only fit if the bag has room; anything left over is paid in coins instead
        int give = p.inv.add(itemMat(quest.mat), quest.matN);
        if (give < quest.matN) p.coins += (quest.matN - give) * MAT_PRICE[quest.mat];
        reward += " + " + S(quest.matN) + " " + MAT_NAME[quest.mat];
    }
    if (quest.bandages > 0) {
        p.inv.add(ITEM_CONS0 + C_BANDAGE, quest.bandages);
        reward += " + " + S(quest.bandages) + " BANDAGE";
    }
    msg("QUEST COMPLETE! " + reward, C_GOLD);
    ftext(p.pos + V2(0, -30), "+" + S(quest.coins) + " COINS", C_GOLD, 1.6f);
    audio.play(S_CRAFT);
    questIdx++;
    questActive = false;
    questProg = 0;
    refreshQuest();
    save();
}
