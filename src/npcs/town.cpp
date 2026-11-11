// town.cpp - walking into the town's buildings and out again, and finding who and what is in reach inside.
//
// Entering parks everything of the outside (the map's state goes into mapMem as for any map change, the wildlife into extAnimals) and
// generates the interior (interiors.hpp) into the world; leaving regenerates the outside from the world seed, puts the parked state back
// and stands the hunter on the exact spot they entered from. The hunter's own state (pack, equipment, stats, skills, quests, time) is
// never touched by going in or out.
#include <algorithm>
#include "core/game.hpp"

namespace {
const char* NPC_NAME[BLD_COUNT] = {"BROKK", "HILD", "TOBIN", "GARRICK", "MARY ROSE", "SALONA MIA"};
const char* NPC_VERB[BLD_COUNT] = {"CRAFT", "CRAFT", "CRAFT", "SHOP", "FLOWERS", "STYLE"};
// What the shopkeepers say while the hunter is in their shop: line 0 is the greeting, the others are idle remarks (all true to the game).
const char* NPC_SAY[BLD_COUNT][4] = {
    {"WELCOME! MIND THE HOT METAL.", "GOOD STEEL TAKES PATIENCE.", "AN AXE IS ONLY AS GOOD AS ITS SMITH.", "BRING ME BARS, I'LL MAKE THEM RING."},
    {"WELCOME, HUNTER! GOT ANY HIDES?", "FRESH PELTS MAKE FINE LEATHER.", "SKIN IT CLEAN AND WASTE NOTHING.", "A GOOD CURED HIDE LASTS YEARS."},
    {"AH, A CUSTOMER! COME IN!", "AXES FOR TREES, PICKS FOR ORE.", "MIND THE SAWDUST!", "A GOOD TOOL SAVES A GOOD DAY."},
    {"GOOD HUNTING! WHAT CAN I DO?", "FRESH HIDES? I PAY WELL!", "SHARP KNIVES, FAIR PRICES!", "SELL WHAT YOU CARRY, LIGHTEN THE LOAD."},
    {"WELCOME TO THE FLOWER SHOP!", "A BOUQUET MAKES A LOVELY GIFT.", "TRADERS PAY WELL FOR BOUQUETS.", "ROSES TODAY, TULIPS TOMORROW."},
    {"COME IN, COME IN! SIT A MOMENT.", "A NEW LOOK FOR THE HUNT?", "LET ME TIDY THAT HAIR.", "EVEN HUNTERS DESERVE A FRESH STYLE."},
};
// What the townsfolk say when the hunter walks past.
const char* VILLAGER_SAY[] = {
    "GOOD DAY, HUNTER!", "THE PORTAL LEADS TO YOUR HOMESTEAD.", "ELDER HOLT KNOWS THE HUNTING QUESTS.", "THE BEST HUNTS ARE PAST THE GATES.",
    "SOME BEASTS ARE BOLDER AT NIGHT.", "TRY THE FLOWER SHOP FOR A GIFT.", "NICE WEATHER FOR A WALK.", "MARY ROSE'S BOUQUETS SELL WELL.",
};
constexpr int N_VILLAGER_SAY = (int)(sizeof(VILLAGER_SAY) / sizeof(VILLAGER_SAY[0]));
}  // namespace
const char* npcName(int k) { return NPC_NAME[k]; }

bool Game::nearShop() const { return interior >= 0; }

int Game::doorNear() const {
    if (interior >= 0 || world.mapId != B_HOME || p.dead) return -1;
    for (int k = 0; k < BLD_COUNT; k++)
        if (dist(p.pos, V2(bldDoorX(k, COX), bldDoorY(k, COY))) < BLD_DOOR_REACH) return k;
    return -1;
}

bool Game::nearExit() const { return interior >= 0 && dist(p.pos, world.exitPos) < 20.f; }

int Game::npcNear() const {
    if (interior < 0) return -1;
    int best = -1;
    float bd = 40.f;
    for (size_t i = 0; i < world.npcs.size(); i++) {
        const float d = dist(p.pos, world.npcs[i].pos);
        if (d < bd) bd = d, best = (int)i;
    }
    return best;
}

bool Game::stationNear() const {
    if (interior < 0) return false;
    for (const V2& s : world.stations)
        if (dist(p.pos, s) < 26.f) return true;
    return false;
}

bool Game::enterBuilding(int k) {
    if (k < 0 || k >= BLD_COUNT || interior >= 0 || p.dead) return false;
    npcSayT = 0;
    npcChatCd = 0.7f;  // the shopkeeper greets the hunter a moment after they step in
    npcSayLine = -1;
    stashOutside();
    world.generateInterior(k);
    initHarvest();
    interior = k;
    p.pos = world.spawn;
    p.kb = V2();
    p.iceV = V2();
    p.charging = false;
    p.gathering = false;
    p.gatherT = 0;
    hvKey = -1;
    bldCd = 0.4f;
    travelFade = 0.4f;
    setCam(0, true);
    audio.play(S_CLICK, 0.6f);
    return true;
}

bool Game::exitBuilding() {
    if (interior < 0) return false;
    const int map = extMap;
    structs.clear();
    projs.clear();
    parts.clear();
    ftexts.clear();
    items.clear();
    loadMapWorld(map);
    restoreMap();
    animals = std::move(extAnimals);
    extAnimals.clear();
    interior = -1;
    p.pos = extPos;
    p.kb = V2();
    p.iceV = V2();
    hvKey = -1;
    bldCd = 0.4f;
    travelFade = 0.4f;
    setCam(0, true);
    audio.play(S_CLICK, 0.6f);
    return true;
}

bool Game::townInteract() {
    if (bldCd > 0 || p.dead) return false;
    if (interior < 0) {
        const int k = doorNear();
        return k >= 0 && enterBuilding(k);
    }
    if (nearExit()) return exitBuilding();
    if (npcNear() >= 0 || stationNear()) {
        openService(interior);
        return true;
    }
    return false;
}

bool Game::townPrompt() {
    if (interior < 0) {
        const int k = doorNear();
        if (k < 0) return false;
        drawPrompt(bldDoorX(k, COX), bldDoorY(k, COY) - 6, "E", std::string("ENTER ") + BUILDINGS[k].name);
        return true;
    }
    if (nearExit()) {
        drawPrompt(world.exitPos.x, world.exitPos.y - 30, "E", "LEAVE");
        return true;
    }
    const int n = npcNear();
    if (n >= 0) {
        drawPrompt(world.npcs[n].pos.x, world.npcs[n].pos.y - 34, "E", std::string(NPC_VERB[interior]) + " WITH " + NPC_NAME[interior]);
        return true;
    }
    if (stationNear()) {
        drawPrompt(p.pos.x, p.pos.y - 30, "E", NPC_VERB[interior]);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- strolling townsfolk and shop talk
void Game::initVillagers() {
    villagers.clear();
    villagersInit = true;
    if (world.mapId != B_HOME) return;
    const int x0 = COX + TOWN_X0, y0 = COY + TOWN_Y0, x1 = COX + TOWN_X1, y1 = COY + TOWN_Y1;
    for (int tries = 0; tries < 600 && (int)villagers.size() < 5; tries++) {
        const int tx = x0 + (int)(frng.range(0.f, 1.f) * (x1 - x0)), ty = y0 + (int)(frng.range(0.f, 1.f) * (y1 - y0));
        if (tx < x0 || ty < y0 || tx >= x1 || ty >= y1 || world.tile[ty][tx] != T_COBBLE) continue;
        const V2 c(tx * (float)TS + 8.f, ty * (float)TS + 8.f);
        if (world.blocked(c.x, c.y, 6.f) || dist(c, p.pos) < 40.f) continue;
        bool crowded = false;
        for (auto& o : villagers) crowded = crowded || dist(o.pos, c) < 60.f;
        if (crowded) continue;
        Villager v;
        v.pos = v.target = c;
        v.look = (int)villagers.size();
        v.idleT = frng.range(0.5f, 4.f);
        v.sayIdx = (v.look * 3) % N_VILLAGER_SAY;
        v.chatCd = frng.range(0.f, 6.f);
        villagers.push_back(v);
    }
}

void Game::updateVillagers(float dt) {
    if (world.mapId != B_HOME || interior >= 0) return;
    if (!villagersInit) initVillagers();
    const int x0 = COX + TOWN_X0, y0 = COY + TOWN_Y0, x1 = COX + TOWN_X1, y1 = COY + TOWN_Y1;
    static const float offs[5] = {0, 0.6f, -0.6f, 1.2f, -1.2f};
    for (auto& v : villagers) {
        v.sayT -= dt;
        v.chatCd -= dt;
        const float dp = dist(v.pos, p.pos);
        if (dp < 34.f && !p.dead) {  // the hunter walks past: stop, turn and say something
            v.faceLeft = p.pos.x < v.pos.x;
            v.moving = false;
            v.idleT = std::max(v.idleT, 0.4f);
            if (v.chatCd <= 0) {
                v.sayT = 3.4f;
                v.sayIdx = (v.sayIdx + 1) % N_VILLAGER_SAY;
                v.chatCd = frng.range(14.f, 22.f);
            }
        }
        if (v.idleT > 0) {
            v.idleT -= dt;
            v.moving = false;
            continue;
        }
        if (!v.hasTarget) {  // choose a short stroll on the cobbles
            for (int k = 0; k < 10 && !v.hasTarget; k++) {
                const float a = frng.range(0.f, 6.2832f), r = frng.range(28.f, 90.f);
                const V2 t = v.pos + V2(cosf(a), sinf(a)) * r;
                const int tx = (int)(t.x / TS), ty = (int)(t.y / TS);
                if (tx < x0 || ty < y0 || tx >= x1 || ty >= y1 || world.tile[ty][tx] != T_COBBLE || world.blocked(t.x, t.y, 6.f)) continue;
                v.target = t;
                v.hasTarget = true;
            }
            if (!v.hasTarget) v.idleT = 2.f;
            continue;
        }
        const V2 d = v.target - v.pos;
        const float L = d.len();
        if (L < 3.f) {  // arrived: stand a while
            v.hasTarget = false;
            v.idleT = frng.range(1.5f, 5.5f);
            v.moving = false;
            continue;
        }
        const float base = atan2f(d.y, d.x);
        bool moved = false;
        for (int k = 0; k < 5 && !moved; k++) {
            const V2 dir(cosf(base + offs[k]), sinf(base + offs[k]));
            const V2 np = v.pos + dir * (24.f * dt);
            const int tx = (int)(np.x / TS), ty = (int)(np.y / TS);
            if (tx < x0 || ty < y0 || tx >= x1 || ty >= y1 || world.tile[ty][tx] != T_COBBLE || world.blocked(np.x, np.y, 5.f)) continue;
            bool bump = false;  // do not walk through each other or the hunter
            for (auto& o : villagers) bump = bump || (&o != &v && dist(o.pos, np) < 9.f && dist(o.pos, np) < dist(o.pos, v.pos));
            if (bump || dist(p.pos, np) < 9.f) continue;
            v.pos = np;
            v.faceLeft = dir.x < -0.05f ? true : dir.x > 0.05f ? false : v.faceLeft;
            moved = true;
        }
        v.moving = moved;
        v.animT += moved ? dt : 0.f;
        if (!moved) {  // boxed in: look around for another way
            v.hasTarget = false;
            v.idleT = 0.8f;
        }
    }
}

void Game::updateShopTalk(float dt) {
    npcSayT -= dt;
    npcChatCd -= dt;
    if (interior < 0 || world.npcs.empty() || p.dead) return;
    if (dist(p.pos, world.npcs[0].pos) < 80.f && npcChatCd <= 0) {
        npcSayLine = (npcSayLine + 1) % 4;  // 0 = the greeting, then the idle remarks in turn
        npcSayT = 3.4f;
        npcChatCd = 11.f;
    }
}

void Game::drawSpeech(float wx, float wy, const std::string& t, float life) const {
    (void)life;
    const int sx = (int)wx - camX, sy = (int)wy - camY;
    const int w = textW(t) + 10, h = 12;
    const int x = std::max(2, std::min(LW - w - 2, sx - w / 2)), y = std::max(2, sy - h);  // kept on screen
    const_cast<Game*>(this)->panel(x, y, w, h, HEX(0xf2e8d0));
    const_cast<Game*>(this)->text(x + 5, y + 4, t, HEX(0x2a1c10), 1);
    const_cast<Game*>(this)->fillRect(sx - 1, y + h, 3, 1, UI_BORDER);
    const_cast<Game*>(this)->fillRect(sx, y + h + 1, 1, 1, UI_BORDER);
}

void Game::drawSpeechBubbles() {
    if (interior >= 0 && !world.npcs.empty() && npcSayT > 0 && interior < BLD_COUNT) {
        const V2 n = world.npcs[0].pos;
        drawSpeech(n.x, n.y - 50, NPC_SAY[interior][npcSayLine & 3], npcSayT);
        return;
    }
    if (world.mapId != B_HOME || interior >= 0) return;
    if (elderSayT > 0) drawSpeech(world.questGiver.x, world.questGiver.y - 58, elderSpeech(), elderSayT);
    for (auto& v : villagers)
        if (v.sayT > 0) drawSpeech(v.pos.x, v.pos.y - 28, VILLAGER_SAY[v.sayIdx % N_VILLAGER_SAY], v.sayT);
}

// ---------------------------------------------------------------- Elder Holt
// He keeps to the cobbles within a few steps of the spot worldgen gave him (the same formula as worldgen.hpp: questGiver on the home map).
const char* Game::elderSpeech() const {
    static const char* idle[] = {"A GOOD HUNTER IS A PATIENT ONE.", "THE WILD GIVES TO THOSE WHO LEARN IT.", "MIND THE BEASTS AFTER DARK.", "YOUR PACK IS HEAVY - TRAVEL LIGHT."};
    if (questReady()) return elderLine % 2 ? "WELL DONE! COME, TELL ME ALL." : "THE HUNT IS DONE - COME AND SEE ME.";
    if (!questActive) return elderLine % 2 ? "I HAVE A NEW HUNT FOR YOU." : "READY FOR ANOTHER HUNT? TALK TO ME.";
    return elderLine % 3 == 0 ? "HOW GOES THE HUNT?" : idle[elderLine % 4];
}

void Game::updateElder(float dt) {
    if (world.mapId != B_HOME || interior >= 0) return;
    const V2 home((COX + 45.75f) * TS, (COY + 31.6f) * TS);
    elderSayT -= dt;
    elderChatCd -= dt;
    const float dp = dist(p.pos, world.questGiver);
    if (dp < 70.f && !p.dead) {
        elderFaceLeft = p.pos.x < world.questGiver.x;
        if (elderChatCd <= 0) {  // the hunter is near: he speaks
            elderSayT = 3.8f;
            elderLine++;
            elderChatCd = 13.f;
        }
        if (dp < 44.f) {
            elderMoving = false;  // he stands still for someone who has come to talk
            return;
        }
    }
    if (elderIdle > 0) {
        elderIdle -= dt;
        elderMoving = false;
        return;
    }
    if (!elderHasTarget) {
        for (int k = 0; k < 10 && !elderHasTarget; k++) {
            const float a = frng.range(0.f, 6.2832f), r = frng.range(8.f, 22.f);
            const V2 t = home + V2(cosf(a), sinf(a)) * r;
            const int tx = (int)(t.x / TS), ty = (int)(t.y / TS);
            if (world.tile[ty][tx] != T_COBBLE || world.blocked(t.x, t.y, 6.f)) continue;
            elderTarget = t;
            elderHasTarget = true;
        }
        if (!elderHasTarget) elderIdle = 3.f;
        return;
    }
    const V2 d = elderTarget - world.questGiver;
    const float L = d.len();
    if (L < 2.5f) {
        elderHasTarget = false;
        elderIdle = frng.range(3.f, 8.f);
        elderMoving = false;
        return;
    }
    const V2 np = world.questGiver + d * (1.f / L) * (16.f * dt);  // a slow old man's pace
    if (world.blocked(np.x, np.y, 5.f) || dist(np, p.pos) < 10.f || dist(np, home) > 30.f) {
        elderHasTarget = false;
        elderIdle = 1.f;
        elderMoving = false;
        return;
    }
    world.questGiver = np;
    elderFaceLeft = d.x < 0;
    elderMoving = true;
    elderAnimT += dt;
}
