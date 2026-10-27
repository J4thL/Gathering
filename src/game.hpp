// game.hpp - entities and the Game class.
#pragma once
#include <deque>
#include <string>
#include <vector>
#include "audio.hpp"
#include "inventory.hpp"
#include "quests.hpp"
#include "sprites.hpp"
#include "world.hpp"
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
    Inventory inv{PACK_SLOTS[0]};  // everything the hunter carries (materials, weapons, ammo, bandages)
    int sel = 0;
    int up[U_COUNT] = {0};
    float gatherT = 0;
    bool gathering = false;
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
    int bagUsed() const { return inv.usedSlots(); }  // slots in use
    int bagCap() const { return inv.capacity(); }   // slots the backpack provides
    void syncPack() { inv.setCapacity(PACK_SLOTS[up[U_BAG]]); }
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

enum State { ST_TITLE, ST_PLAY, ST_SHOP, ST_PAUSE, ST_QUEST, ST_INVENTORY };

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
    int invTab = 0, invSel = 0;   // inventory screen: 0 = backpack, 1 = crafting; selected slot
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
    // test hooks
    bool botOn = false;

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
    bool useSlot(int slot);  // inventory screen: equip a weapon / use a bandage
    void updateInventory(float dt);
    void hitAnimal(Animal& a, float dmg, V2 dir, bool crit);
    void killAnimal(Animal& a);
    void hurtPlayer(float dmg, V2 from);
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
    bool nearShop() const;
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
    bool craft(int r);
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
