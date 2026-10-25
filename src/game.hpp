// game.hpp - entities and the Game class.
#pragma once
#include <deque>
#include <string>
#include <vector>
#include "audio.hpp"
#include "sprites.hpp"
#include "world.hpp"

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
    bool mapToggle = false, helpToggle = false;
    bool uiClick = false;  // mouse click latched for UI buttons; cleared after each rendered frame
};

enum AState { AS_WANDER, AS_FLEE, AS_CHASE };
struct Animal {
    int kind = 0, variant = 0;
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
};

struct Player {
    V2 pos, kb;
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
    int mats[M_COUNT] = {0};
    int wpn[W_COUNT] = {0};
    int sel = 0;
    int up[U_COUNT] = {0};
    int bandages = 0;
    float gatherT = 0;
    bool gathering = false;
    float gatherSfx = 0;
    bool dead = false;
    float deadT = 0;
    float throwAnim = 0;
    V2 aim = V2(1, 0);
    int bagUsed() const {
        int n = 0;
        for (int m : mats) n += m;
        return n;
    }
    int bagCap() const { return BAG_CAP[up[U_BAG]]; }
};

struct Proj {
    V2 pos, dir, last;
    float speed, travelled, range, angle, spin, dmg;
    int wpn;
    bool crit;
};
struct GroundItem {
    V2 pos;
    int wpn;
    float angle, t;
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

enum State { ST_TITLE, ST_PLAY, ST_SHOP, ST_PAUSE };

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
    Rng rng{20261024};
    float timeAcc = 0, dayT = 0, respawnT = 0, helpT = 0, bagWarn = 0;
    V2 cam;
    float shakeAmt = 0;
    int camX = 0, camY = 0;
    int shopTab = 0;
    bool showMap = true;
    bool hasSave = false;
    int kills = 0, sold = 0;
    float titleT = 0;
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
    void updateFx(float dt);
    void throwWeapon();
    void hitAnimal(Animal& a, float dmg, V2 dir, bool crit);
    void killAnimal(Animal& a);
    void hurtPlayer(float dmg, V2 from);
    void gatherStep(Animal& c);
    bool stepAnimal(Animal& a, V2 dir, float speed, float dt);
    bool trySpawn(int kind, float minDist, bool offscreen);
    void populate();
    Animal* nearestCarcass(float maxD);
    float tod() const;
    float daylight() const;
    void msg(const std::string& s, u32 col = C_TEXT);
    void ftext(V2 p, const std::string& s, u32 col, float life = 1.1f);
    void bloodBurst(V2 p, V2 dir, int n);
    void puff(V2 p, int n, u32 col);
    void addItem(int m, int n);
    bool nearShop() const;
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
    void drawTitle();
    void drawPause();
    void drawReticle();
    void drawMinimap();
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
