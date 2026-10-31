// game.hpp - entities and the Game class.
#pragma once
#include <deque>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>
#include "audio.hpp"
#include "inventory.hpp"
#include "quests.hpp"
#include "sprites.hpp"
#include "world.hpp"
#include "resources.hpp"
#include "skills.hpp"
#include "spawns.hpp"

// ----------------------------------------------------------------- palette
const u32 C_TEXT = HEX(0xf3e9d2), C_DIM = HEX(0xa8977a), C_GOLD = HEX(0xf2c94c), C_RED = HEX(0xe8605a),
          C_GREEN = HEX(0x86d85e), C_BLUE = HEX(0x7ab8f0), C_ORANGE = HEX(0xf0953a);
const u32 UI_BG = HEX(0x2b1d12), UI_MID = HEX(0x4a3524), UI_BORDER = HEX(0x140d07), UI_BTN = HEX(0x6a4a2a),
          UI_BTN_HOV = HEX(0x92693a), UI_BTN_OFF = HEX(0x3a2d22);

inline std::string S(int v) { return std::to_string(v); }

struct Input {
    float mx = 200, my = 112;
    bool l = false, lPressed = false, lReleased = false;
    bool up = false, down = false, left = false, right = false, sprint = false, e = false;
    bool ePressed = false, escPressed = false, tabPressed = false, enterPressed = false, bandage = false;
    int selectKey = -1, wheel = 0;
    bool mapToggle = false, helpToggle = false, invToggle = false;
    bool uiClick = false;  // mouse click latched for UI buttons; cleared after each rendered frame
};

enum AState { AS_WANDER, AS_FLEE, AS_CHASE, AS_FEED, AS_REST };
struct Animal {
    int kind = 0, variant = 0;
    int biome = 0;  // the biome it lives in; animals don't wander across biome borders
    V2 pos, vel, kb;
    int hp = 1, maxhp = 1;
    bool dead = false;
    float deadT = 0;
    int face = 1;
    AState st = AS_WANDER;
    float stT = 0, idleT = 0, animT = 0, flash = 0, atkCd = 0, hpShow = 0, bias = 1, noticeT = 0;
    V2 target;
    int remain[M_COUNT] = {0};
    int totalRemain = 0;
    float fade = 1;  // carcass fade-out
    float bodyH = 6;
    bool gone = false;
    int tier = 0;         // 0 common .. 3 legendary (see fauna.hpp)
    float scale = 1.f;    // body scale (tier)
    float air = 0.f;      // height above the ground (flyers)
};

struct Player {
    V2 pos, kb, iceV;  // iceV: momentum while sliding on ice
    int dir = 0;
    bool flip = false;
    float animT = 0;
    float hp = 100, maxhp = 100, stamina = 100;
    bool exhausted = false;
    float staDelay = 0;
    float charge = 0;
    bool charging = false;
    float cd = 0, invuln = 0, sinceHurt = 99;
    int coins = 0;
    HunterLook look;      // appearance (salon.cpp): hairstyle, hair colour, skin, tunic
    Inventory inv;  // everything the hunter carries: always 100 slots, limited by weight (the worn bag sets the limit)
    int bag = -1;   // the worn bag (a PackId), -1 = none; it is not in the inventory while worn and its weight does not count
    int hot[HOT_N] = {-1, -1, -1, -1, -1, -1, -1, -1};  // hotbar: the item equipped in each quick slot (item id), -1 = empty
    int sel = 0;
    int up[U_COUNT] = {0};
    SkillSet sk;  // skill XP (skills.hpp); saved as "skill <key> <xp>" lines
    float gatherT = 0;
    bool gathering = false;
    float swiftT = 0, nightT = 0;  // velvet tonic (faster) / nightsight tonic (see in the dark); not saved
    float warmT = 0, coolT = 0;  // seconds left of a warm brew (no cold drain) / cactus water (no heat drain); not saved
    float pickT = 0;              // hold-E progress for picking up a placed campfire
    float gatherSfx = 0;
    bool dead = false;
    float deadT = 0;
    float throwAnim = 0;
    V2 aim = V2(1, 0);
    // read-only views of what is in the inventory
    int mat(int m) const { return inv.count(itemMat(m)); }
    int wpn(int w) const { return inv.count(itemWpn(w)); }
    int ammo(int a) const { return inv.count(itemAmmo(a)); }
    int bandages() const { return inv.count(ITEM_CONS0 + C_BANDAGE); }
    bool has(int carried) const { return inv.count(itemCarry(carried)) > 0; }  // carried gear is active while it is in the pack
    int healItems() const {  // everything the Q key can use
        int n = 0;
        for (int c : {C_BANDAGE, C_SALVE, C_JERKY, C_COOKED, C_POULTICE, C_TONIC, C_STEW, C_MEDICINE}) n += inv.count(ITEM_CONS0 + c);
        return n;
    }
    int bagUsed() const { return inv.usedSlots(); }  // slots in use
    int bagCap() const { return inv.capacity(); }   // always 100
    int carryLimit() const { return bag >= 0 ? PACK_CARRY_KG[bag] : BASE_CARRY_KG; }  // kg
    void syncPack() { inv.maxWeight = (float)carryLimit(); }  // call after the worn bag changes
    int hotSlotOf(int itemId) const {
        for (int i = 0; i < HOT_N; i++)
            if (hot[i] == itemId) return i;
        return -1;
    }
    bool equippedWpn(int w) const { return hotSlotOf(itemWpn(w)) >= 0; }
};

struct Proj {
    V2 pos, dir, last;
    float speed, travelled, range, angle, spin, dmg;
    int wpn;
    bool crit;
    int ammo = -1;  // >= 0: an arrow / bolt fired from a launcher
};
struct GroundItem {
    V2 pos;
    int wpn;
    float angle, t;
    int ammo = -1;  // >= 0: a recovered arrow / bolt lying on the ground (wpn is then unused)
    int item = -1;  // >= 0: a stack the hunter dropped (any item id, `n` pieces); wpn / ammo are then unused
    int n = 0;
    float lock = 0; // seconds before a dropped stack can be picked up again (so it is not grabbed back at once)
};
struct Part {
    float x, y, z, vx, vy, vz, life, max;
    u32 col;
    int size;
    bool grav;
};
struct FText {
    V2 p;
    std::string s;
    u32 col;
    float t, life;
};
struct Decal {
    V2 p;
    int size;
    float t, life;
};
struct Msg {
    std::string s;
    u32 col;
    float t;
};
struct Fly {
    float x, y, ph, sp;
};
struct Flake {  // screen-space weather particle (snow / blown sand / gnats / wind dust)
    float x, y, vx, vy, ph, sz;
};

// harvest state of one resource (see gathering.cpp): hits already landed, regrow timers, a little shake when struck
struct HState {
    uint8_t done = 0;   // swings landed so far
    bool gone = false;  // finished: shown as a stump / rubble / stub until it regrows
    float t = 0;        // gone: seconds until it starts to regrow; damaged: seconds since the last hit
    float fade = 0;     // regrowing: seconds of sprout left
    float shake = 0;
};
struct HTarget {
    bool valid = false;
    bool node = false;  // world.nodes (true) or world.props (false)
    int idx = -1, res = -1;
    V2 pos;             // where the resource is (aim point)
    float dist = 0;
    int key() const { return valid ? (node ? (1 << 28) : 0) | idx : -1; }
};
constexpr int CHEST_SLOTS = 20, MAX_STRUCTS = 16;
constexpr int INV_COLS = 10;  // the pack screen draws its 100 slots as 10 columns x 10 rows
// things the hunter builds: campfires (smelting, cooking, warmth, light) and storage chests
struct Structure {
    int kind = PL_CAMPFIRE;
    int tx = 0, ty = 0;
    Inventory inv{CHEST_SLOTS, 1e9f};  // chests only (a chest has no weight limit)
    V2 pos() const { return V2(tx * TS + 8.f, ty * TS + 12.f); }
};

enum State { ST_TITLE, ST_PLAY, ST_SHOP, ST_PAUSE, ST_QUEST, ST_INVENTORY, ST_CHEST, ST_SERVICE };

struct Game {
    // platform
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* canvas = nullptr;
    SDL_Texture* miniTex = nullptr;
    float presScale = 3, offX = 0, offY = 0;
    bool running = true;
    bool fullscreen = false;
    // content
    Assets A;
    Audio audio;
    World world;
    Input in;
    State state = ST_TITLE;
    Player p;
    std::vector<Animal> animals;
    std::vector<GroundItem> items;
    std::vector<Proj> projs;
    std::vector<Part> parts;
    std::vector<FText> ftexts;
    std::vector<Decal> decals;
    std::deque<Msg> msgs;
    std::vector<Fly> flies;
    std::vector<Flake> flakes;
    Rng rng{20261024};
    Rng frng{424243};  // wildlife extras (tiers, forms of new species, rest/feed) - the original rng sequence is untouched
    float legCool = 120.f, spawnT = 0.f, noAmmoT = 0.f;
    int shopPage = 0;
    int invTab = 0, invSel = 0;   // inventory screen: 0 = backpack, 1 = crafting, 2 = skills; selected slot
    // gathering (gathering.cpp)
    std::vector<HState> propSt, nodeSt;  // one per world.props / world.nodes entry
    std::vector<int8_t> propRsId;        // ResId of each prop, -1 = scenery
    std::vector<int> hActive;            // encoded ids (node flag | index) of resources that are damaged, gone or regrowing
    int hvKey = -1;                      // the resource the current swing belongs to
    float hvMsgT = 0;                    // rate limit for "need a better tool" messages
    Rng grng{909091};                    // gathering rolls and particles: its own stream, so the hunting sequence is untouched
    // crafting screen (crafting.cpp)
    int sellScroll = 0, invFilter = 0;  // shop SELL list scroll; pack filter chip (0 all, 1 materials, 2 weapons, 3 tools, 4 supplies)
    int craftCat = -1;                   // category filter, -1 = all
    int craftScroll = 0;
    bool craftOnly = false;              // show only what can be made right now
    // structures (structures.cpp)
    std::vector<Structure> structs;
    int chestOpen = -1;
    int chestSel = 0;
    bool navPrev[4] = {false, false, false, false};
    struct LoadReport {
        int version = 0;       // save format that was read (0 = nothing recognisable)
        bool migrated = false;  // true when a version 1 save was converted
        int unknown = 0;       // items in the file this game does not know (skipped)
        int dropped = 0;       // items that did not fit in the inventory (should stay 0)
    } loadReport;
    float spCool[A_COUNT] = {0};
    int spawnStat[A_COUNT][4] = {{0}};  // spawns per species and tier (for the --wildlife test)
    Rng wrng{777001};  // weather only, so biome effects never disturb the gameplay random sequence
    float timeAcc = 0, dayT = 0, respawnT = 0, helpT = 0, bagWarn = 0;
    float stuckCheckT = 0;  // timer for the out-of-weapons safety net
    V2 cam;
    float shakeAmt = 0;
    int camX = 0, camY = 0;
    int shopTab = 0;
    bool showMap = true;
    bool hasSave = false;
    int kills = 0, sold = 0;
    float titleT = 0;
    // quests: questIdx is how many are done (= the current quest's number); only these 3 numbers are saved
    int questIdx = 0, questProg = 0;
    bool questActive = false;
    Quest quest;  // cached questAt(questIdx), refreshed whenever questIdx changes
    // biomes
    int curBiome = B_HOME, pendBiome = B_HOME;
    float pendT = 0, bannerT = 0;
    int bannerBiome = B_HOME;
    std::string bannerSub;
    float bw[B_COUNT] = {1, 0, 0, 0, 0};  // smoothed weight of each biome around the player (drives weather and tint)
    // maps (maps.cpp): the world holds the active map; everything that belongs to the other maps is parked in mapMem
    struct MapMem {
        bool visited = false;
        std::vector<std::string> hv, st;  // harvested resources and placed structures, as save lines
        std::string disc;                 // which landmarks were discovered ('1' / '0')
        std::vector<GroundItem> items;    // thrown weapons / arrows lying on the ground
    } mapMem[B_COUNT];
    float travelFade = 0.f, travelCd = 0.f;  // fade-in after a trip (seconds left); no new trip before this runs out
    // town buildings (town.cpp): while inside, `world` holds the interior and everything of the outside is parked here
    int interior = -1;                 // BuildingKind the hunter is inside, -1 = outdoors
    int extMap = B_HOME;               // the map to return to
    V2 extPos;                         // the exact spot the hunter stood on when entering: leaving puts them back there
    std::vector<Animal> extAnimals;    // the outside wildlife, parked while indoors
    float bldCd = 0;                   // no new door use for a moment after one (the E that opened a door must not also close it)
    int svc = -1;                      // ST_SERVICE: the building whose service screen is open
    int craftBld = -1;                 // crafting list filter (-1 = the whole book, as in the pack's CRAFTING tab)
    int svcTab = 0;
    HunterLook salonLook;                    // the salon's preview (not applied until CONFIRM)
    Spr salonPrev[3];                  // preview sprites [dir], drawn for salonPrevLook
    HunterLook salonPrevLook;
    // test hooks
    bool botOn = false;
    bool craftAnywhere = false;  // the crafting tests run away from the town buildings; real play never sets this

    // ---- platform (main.cpp / game_logic.cpp)
    bool init(bool headless);
    void shutdown();
    void present();
    void saveCanvas(const char* path);
    std::string savePath();
    void save();
    bool load();

    // ---- logic (game_logic.cpp)
    void newGame();
    void update(float dt);
    void updatePlay(float dt);
    void updatePlayer(float dt);
    void updateAnimals(float dt);
    void updateProjectiles(float dt);
    void checkWeaponSafetyNet(float dt);
    void updateFx(float dt);
    void updateBiome(float dt);
    void updateWeather(float dt);
    void climateMods(float& drain, float& regen) const;
    int landmarksFound() const;
    void throwWeapon();
    bool canUse(int w) const;
    bool useSlot(int slot);  // inventory screen: equip a weapon / use a consumable / wear a bag
    // ---- equipment: the worn bag and the hotbar (items equipped for quick use)
    bool equipBag(int slot);            // wear the bag in this pack slot (the old one goes back into the pack)
    bool unequipBag();
    bool equipHot(int itemId, int hotSlot = -1);  // put an item on the hotbar (first free compatible slot unless one is given)
    void unequipHot(int hotSlot);
    bool equipHotQuiet(int itemId);  // equipHot without messages or sounds (loading old saves)
    void hotUse(int hotSlot);           // the hotbar key: select a weapon / use a consumable / place a structure
    void fixSel();                      // keeps the selected weapon one that is on the hotbar
    bool dropSlot(int slot, int n);     // put a stack on the ground
    void notifyBlocked(int itemId, int n, const Inventory* ref = nullptr);  // "PACK FULL" / "TOO HEAVY" feedback after a failed pickup
    bool canUseWpn(int w) const { return canUse(w); }
    bool useConsumable(int c);
    void updateInventory(float dt);
    void hitAnimal(Animal& a, float dmg, V2 dir, bool crit);
    void killAnimal(Animal& a);
    float skillBonus(int skill) const { return skillBonusAt(skill, p.sk.level(skill)); }  // see skills.hpp
    void hurtPlayer(float dmg, V2 from, int srcKind = -1);
    void gatherStep(Animal& c);
    bool stepAnimal(Animal& a, V2 dir, float speed, float dt);
    bool trySpawn(int kind, float minDist, bool offscreen);
    void populate();
    Animal* spawnAnimal(int kind, V2 pos, int form, int tier);
    void streamSpawns(float dt);
    int liveLegendaries() const;
    int rollTier();
    void applyTier(Animal& a, int tier);
    Animal* nearestCarcass(float maxD);
    float tod() const;
    float daylight() const;
    void msg(const std::string& s, u32 col = C_TEXT);
    void ftext(V2 p, const std::string& s, u32 col, float life = 1.1f);
    void bloodBurst(V2 p, V2 dir, int n);
    void puff(V2 p, int n, u32 col);
    void addItem(int m, int n);
    // ---- maps (maps.cpp)
    bool travelTo(int map, int atx, int aty, bool autosave = true);  // switch the active map; the hunter keeps everything
    void loadMapWorld(int map);        // regenerate the active world as `map` (no game state changes)
    void snapshotMap();                // park the active map's state in mapMem
    void restoreMap();                 // put mapMem's state for the active map back
    V2 freeSpot(int tx, int ty) const;  // the nearest standable position to a tile
    void checkPortals(float dt);
    void drawPortalLabels();
    // ---- gathering (gathering.cpp)
    void initHarvest();   // size the state tables for the current world (call after world.generate)
    void resetHarvest();  // everything grown back (new game)
    bool findHarvest(HTarget& out) const;
    int bestTool(int type, int* toolIdx = nullptr) const;  // highest tool tier of a type in the pack (0 = none)
    bool harvestUp(bool node, int idx) const;
    HState& hstate(bool node, int idx) { return node ? nodeSt[idx] : propSt[idx]; }
    const HState& hstate(bool node, int idx) const { return node ? nodeSt[idx] : propSt[idx]; }
    bool lineClear(V2 a, V2 b, int ignoreTx, int ignoreTy) const;
    void harvestStep(const HTarget& t, float dt);  // hold-E gathering: advances the swing, lands hits, finishes the resource
    void harvestFinish(const HTarget& t);
    void updateHarvest(float dt);
    void harvestChips(V2 at, u32 col, int n, float power);
    void harvestSave(std::ostream& f) const;
    void harvestLoad(const std::vector<std::string>& lines);
    void drawHarvestPrompt();
    std::string harvestVerb(int res) const;
    // ---- structures (structures.cpp)
    bool placeStructure(int kind);
    bool structInteract(float dt);
    void drawStructPrompt();
    bool canPlaceAt(int tx, int ty) const;
    Structure* structNear(float r, int kind = -1);
    bool nearFire() const;
    float warmthAt(V2 pos) const;  // 0..1, how warm the spot is from nearby campfires
    void updateStructures(float dt);
    void structSave(std::ostream& f) const;
    void structLoad(const std::vector<std::string>& lines);
    void updateChest(float dt);
    void drawChest();
    bool chestMove(int fromPack, int slot);
    // ---- crafting (crafting.cpp)
    bool craft(int r);
    bool nearStation(int station) const;
    const Spr& itemIcon(int id) const;
    std::string craftBlock(int r) const;  // "" when the recipe can be made now, otherwise why not (short, upper case)
    bool hasIngredients(const Recipe& R) const;
    void drawCraftTab(int X, int Y, int W, int H);
    // skills (skills.cpp)
    int grantXp(int skill, int amount);          // adds XP, shows the toast, announces level-ups; returns levels gained
    void skillSave(std::ostream& f) const;
    void skillLoadLine(std::istringstream& ss);  // the rest of a "skill <key> <xp>" line (the word "skill" is already read)
    void drawSkillsTab(int X, int Y, int W, int H);
    bool nearShop() const;   // true while inside any town building (the shelter heals quickly)
    // ---- town buildings (town.cpp, services.cpp)
    int doorNear() const;               // BuildingKind whose door is within reach (outdoors), or -1
    bool nearExit() const;              // standing on the doormat inside
    int npcNear() const;                // index in world.npcs of the person within talking reach, or -1
    bool stationNear() const;           // next to the anvil / rack / workbench of the building
    bool enterBuilding(int k);
    bool exitBuilding();
    bool townInteract();                // the E key in town: enter, leave or open the service; true when it acted
    void openService(int k);
    void closeService();
    void updateService(float dt);
    bool townPrompt();                  // draws the door / person / exit prompt in reach; true when one was drawn
    void drawService();
    void drawFlowerShop();
    void drawSalon();
    void openSalon();
    bool confirmSalon();
    bool buyFlower(int m, int n);
    bool applyLook(const HunterLook& l);      // redraw the hunter's real sprites with this look
    void drawCraftService();            // blacksmith / leatherworks / workshop
    void drawUpgradeRows(int x0, int Y, int cw, const int* list, int n);
    // quest giver
    bool nearQuestGiver() const;
    bool questReady() const { return questActive && questProg >= quest.goal; }
    void refreshQuest();
    void questEvent(int type, int id, int n);
    void acceptQuest();
    void turnInQuest();
    // shop actions
    bool canAfford(int coins, int m1, int n1, int m2, int n2) const;
    void spend(int coins, int m1, int n1, int m2, int n2);
    bool buyUpgrade(int u);
    void sellMat(int m, int n);

    // ---- rendering (game_render.cpp)
    void render();
    void renderWorld(bool showPlayerPrompts);
    void drawHud();
    void drawShop();
    void drawQuestPanel();
    void drawInventory();
    void drawTitle();
    void drawPause();
    void drawReticle();
    void drawMinimap();
    void drawBiomeFx();
    void drawBiomeBanner();
    void drawPrompt(float wx, float wy, const std::string& key, const std::string& label);
    // 2d primitives on canvas
    void fillRect(int x, int y, int w, int h, u32 c);
    void sprS(const Spr& s, int sx, int sy, bool flip = false, float alpha = 1.f, double angle = 0, bool pivot = false);
    void sprW(const Spr& s, float wx, float wy, bool flip = false, float alpha = 1.f, double angle = 0, bool pivot = false) {
        sprS(s, (int)floorf(wx) - camX, (int)floorf(wy) - camY, flip, alpha, angle, pivot);
    }
    void icon(const Spr& s, int cx, int cy, int size = 16, float alpha = 1.f);
    void text(int x, int y, const std::string& s, u32 col = C_TEXT, int sc = 1, bool shadow = true, float alpha = 1.f);
    void textC(int cx, int y, const std::string& s, u32 col = C_TEXT, int sc = 1, bool shadow = true);
    void textOutlined(int x, int y, const std::string& s, u32 col, float alpha = 1.f);
    int textW(const std::string& s, int sc = 1) const { return (int)s.size() * 4 * sc - sc; }
    void panel(int x, int y, int w, int h, u32 fill = UI_MID);
    bool inRect(int x, int y, int w, int h) const { return in.mx >= x && in.my >= y && in.mx < x + w && in.my < y + h; }
    bool button(int x, int y, int w, int h, const std::string& label, bool enabled = true, u32 base = UI_BTN);
    int chip(int x, int y, const Spr& ic, int have, int need);
    void bar(int x, int y, int w, int h, float f, u32 fill, u32 back = HEX(0x1a120b));
    void setCam(float dt, bool snap = false);
};
