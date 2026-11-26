// game_settings.cpp - the SETTINGS screen (volumes + rebindable keys), the HELP pages, and loading / saving settings.txt.
#include <algorithm>
#include <fstream>
#include <sstream>
#include "core/game.hpp"

std::string Game::settingsPath() {
    const std::string s = savePath();
    const size_t k = s.find_last_of("/\\");
    return (k == std::string::npos ? std::string() : s.substr(0, k + 1)) + "settings.txt";
}

void Game::saveSettings() {
    std::ofstream f(settingsPath(), std::ios::trunc);
    if (!f) return;
    f << "sfx " << cfg.sfx << "\namb " << cfg.amb << "\nmuted " << (audio.muted ? 1 : 0) << "\nhires2 " << (cfg.hiRes ? 1 : 0) << "\nwindow " << winW << ' ' << winH << "\nfullscreen " << (fullscreen ? 1 : 0) << "\n";
    f << "shake " << cfg.shake << "\nshowfps " << (cfg.showFps ? 1 : 0) << "\n";
    for (int i = 0; i < K_COUNT; i++) f << "key " << i << ' ' << (int)cfg.key[i] << "\n";
}

// The hi-res option decides how the sprites are built, so it is read before they are (see init).
bool Game::readHiResSetting() {
    std::ifstream f(settingsPath());
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string k;
        long long a = 0;
        if (ss >> k >> a && k == "hires2") return a != 0;
    }
    return !getenv("WILDWOOD_SAVE_DIR");  // no choice made yet: hi-res is on for a real run (tests and tools set WILDWOOD_SAVE_DIR and keep the plain canvas)
}

// The window size and fullscreen choice, read before the window is made (see init). Values out of range are ignored.
void Game::readWindowSetting(int& w, int& h, bool& fs) {
    std::ifstream f(settingsPath());
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string k;
        ss >> k;
        long long a = 0, b = 0;
        if (k == "window" && ss >> a >> b && a >= 400 && a <= 16384 && b >= 225 && b <= 16384) { w = (int)a; h = (int)b; }
        else if (k == "fullscreen" && ss >> a) fs = a != 0;
    }
}

void Game::loadSettings() {
    std::ifstream f(settingsPath());
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string k;
        long long a = 0, b = 0;
        ss >> k;
        if (k == "sfx" && ss >> a) cfg.sfx = (int)std::max(0LL, std::min(10LL, a));
        else if (k == "amb" && ss >> a) cfg.amb = (int)std::max(0LL, std::min(10LL, a));
        else if (k == "muted" && ss >> a) audio.muted = a != 0;
        else if (k == "hires2" && ss >> a) cfg.hiRes = a != 0;
        else if (k == "shake" && ss >> a) cfg.shake = (int)std::max(0LL, std::min(10LL, a));
        else if (k == "showfps" && ss >> a) cfg.showFps = a != 0;
        else if (k == "key" && ss >> a >> b && a >= 0 && a < K_COUNT && !keyReserved((SDL_Scancode)b) && b > 0 && b < SDL_NUM_SCANCODES) cfg.key[a] = (SDL_Scancode)b;
    }
    // a hand-edited file must not bind one key to two actions
    for (int i = 0; i < K_COUNT; i++)
        for (int j = i + 1; j < K_COUNT; j++)
            if (cfg.key[i] == cfg.key[j]) cfg.key[j] = KEYINFO[j].def == cfg.key[i] ? SDL_SCANCODE_UNKNOWN : KEYINFO[j].def;
    for (int i = 0; i < K_COUNT; i++)
        if (cfg.key[i] == SDL_SCANCODE_UNKNOWN) cfg.resetKeys();
    audio.sfxVol = cfg.sfx / 10.f;
}

std::string Game::keyName(int act) const {
    std::string s = SDL_GetScancodeName(cfg.key[act]);
    for (char& c : s) c = (char)toupper((unsigned char)c);
    return s.empty() ? "?" : s;
}

// the key a player pressed while a control was waiting for one: Esc cancels, a key already in use swaps with it
void Game::rebindKey(SDL_Scancode sc) {
    const int act = rebind;
    rebind = -1;
    if (sc == SDL_SCANCODE_ESCAPE) return;
    if (keyReserved(sc)) {
        msg("THAT KEY IS RESERVED", C_RED);
        audio.play(S_ERROR, 0.4f);
        return;
    }
    for (int i = 0; i < K_COUNT; i++)
        if (i != act && cfg.key[i] == sc) cfg.key[i] = cfg.key[act];  // swap, so no key ever does two things
    cfg.key[act] = sc;
    saveSettings();
    audio.play(S_CLICK, 0.5f);
}

void Game::openSettings() {
    settingsBack = state;
    state = ST_SETTINGS;
    rebind = -1;
}

void Game::updateSettings(float dt) {
    updateFx(dt);
    if (rebind < 0) {  // left / right turn the pages
        if (in.rightPressed) settingsTab = (settingsTab + 1) % 3;
        if (in.leftPressed) settingsTab = (settingsTab + 2) % 3;
    }
    if (in.escPressed && rebind < 0) {
        state = settingsBack;
        saveSettings();
    }
}

void Game::drawSettings() {
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 170));
    const int X = 40, Y = 8, W = 320, H = 208;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, UI_BG);
    text(X + 8, Y + 7, "SETTINGS", C_GOLD);
    textC(X + W - 56, Y + 7, "< > SWITCH PAGE", C_DIM, 1, false);
    // ---- the pages
    static const char* TABS[3] = {"AUDIO", "VIDEO", "CONTROLS"};
    for (int t = 0; t < 3; t++)
        if (button(X + 10 + t * 76, Y + 22, 72, 14, TABS[t], true, settingsTab == t ? HEX(0x7a5a1e) : HEX(0x3f3220))) {
            settingsTab = t;
            rebind = -1;
            cfgDrag[0] = cfgDrag[1] = cfgDrag[2] = false;
        }
    fillRect(X + 8, Y + 39, W - 16, 1, HEX(0x6a5030));
    const int cy = Y + 48;
    if (settingsTab == 0) {  // ---- audio
        text(X + 10, cy, "SOUND EFFECTS", C_TEXT);
        int v = cfg.sfx + 1;
        sliderBar(X + 110, cy + 3, 150, v, 11, cfgDrag[0]);
        if (cfg.sfx != v - 1) {
            cfg.sfx = v - 1;
            audio.sfxVol = cfg.sfx / 10.f;
        }
        text(X + 270, cy, S(cfg.sfx), C_GOLD);
        text(X + 10, cy + 16, "AMBIENCE", C_TEXT);
        v = cfg.amb + 1;
        sliderBar(X + 110, cy + 19, 150, v, 11, cfgDrag[1]);
        cfg.amb = v - 1;
        text(X + 270, cy + 16, S(cfg.amb), C_GOLD);
        if (button(X + 10, cy + 38, 110, 14, audio.muted ? "SOUND: OFF" : "SOUND: ON", true, audio.muted ? HEX(0x7a3a2a) : HEX(0x4a5a2e))) audio.muted = !audio.muted;
        text(X + 10, cy + 60, "SOUND EFFECTS: HITS, FOOTSTEPS, THE FIRE'S CRACKLE.", C_DIM, 1, false);
        text(X + 10, cy + 70, "AMBIENCE: WIND, RAIN, WATER AND ANIMALS.", C_DIM, 1, false);
    } else if (settingsTab == 1) {  // ---- video
        if (button(X + 10, cy, 134, 14, fullscreen ? "FULLSCREEN: ON" : "FULLSCREEN: OFF", true, fullscreen ? HEX(0x4a5a2e) : HEX(0x3f3220))) {
            fullscreen = !fullscreen;
            SDL_SetWindowFullscreen(win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            saveSettings();
            audio.play(S_CLICK, 0.5f);
        }
        text(X + 152, cy + 4, "(F11 DOES THE SAME)", C_DIM, 1, false);
        // hi-res: sprites and text are drawn with EPX2 smoothing at double resolution; the sprites are built at start-up, so it applies after a restart
        if (button(X + 10, cy + 20, 134, 14, cfg.hiRes ? "HI-RES: ON" : "HI-RES: OFF", true, cfg.hiRes ? HEX(0x4a5a2e) : HEX(0x3f3220))) {
            cfg.hiRes = !cfg.hiRes;
            saveSettings();
            audio.play(S_CLICK, 0.5f);
        }
        if (cfg.hiRes != (LS > 1)) text(X + 152, cy + 24, "RESTART THE GAME TO APPLY", C_ORANGE, 1, false);
        else text(X + 152, cy + 24, "SMOOTHER SPRITES AND TEXT", C_DIM, 1, false);
        if (button(X + 10, cy + 40, 134, 14, cfg.showFps ? "SHOW FPS: ON" : "SHOW FPS: OFF", true, cfg.showFps ? HEX(0x4a5a2e) : HEX(0x3f3220))) {
            cfg.showFps = !cfg.showFps;
            saveSettings();
            audio.play(S_CLICK, 0.5f);
        }
        text(X + 152, cy + 44, "A FRAME COUNTER IN THE CORNER", C_DIM, 1, false);
        text(X + 10, cy + 64, "SCREEN SHAKE", C_TEXT);
        int v = cfg.shake + 1;
        sliderBar(X + 110, cy + 67, 150, v, 11, cfgDrag[2]);
        cfg.shake = v - 1;
        text(X + 270, cy + 64, S(cfg.shake), C_GOLD);
        text(X + 10, cy + 80, cfg.shake == 0 ? "NO SHAKE WHEN YOU ARE HIT OR A BOSS ROARS." : "HOW MUCH THE SCREEN SHAKES ON HITS AND BOSS BLOWS (0 = NONE).", C_DIM, 1, false);
        text(X + 10, cy + 96, "WINDOW: " + S(winW) + " X " + S(winH) + " - THE SIZE IS REMEMBERED", C_DIM, 1, false);
    } else {  // ---- controls
        text(X + 10, cy - 2, "CLICK A KEY, THEN PRESS THE NEW ONE", C_GOLD);
        const int cw = (W - 20) / 2, per = (K_COUNT + 1) / 2;
        for (int i = 0; i < K_COUNT; i++) {
            const int x = X + 10 + (i / per) * (cw + 4), y = cy + 8 + (i % per) * 13;
            text(x, y + 3, KEYINFO[i].label, C_TEXT);
            const std::string nm = rebind == i ? "PRESS A KEY" : keyName(i);
            if (button(x + 72, y, cw - 74, 11, nm, true, rebind == i ? HEX(0x7a5a1e) : HEX(0x3f3220))) rebind = rebind == i ? -1 : i;
        }
        const int fy = cy + 8 + per * 13 + 2;
        textC(X + W / 2, fy, "FIXED: 1-8 HOTBAR, ARROWS, TAB, ENTER, ESC, F11", C_DIM, 1, false);
        if (button(X + 10, Y + H - 22, 90, 14, "DEFAULT KEYS", true, HEX(0x4a3a5e))) {
            cfg.resetKeys();
            rebind = -1;
        }
    }
    if (button(X + W - 70, Y + H - 22, 60, 14, "BACK", true, HEX(0x5d7a2e))) {
        state = settingsBack;
        saveSettings();
    }
    if (!in.l) {
        if (cfgDrag[0] || cfgDrag[1] || cfgDrag[2]) saveSettings();  // (a slider was let go)
        cfgDrag[0] = cfgDrag[1] = cfgDrag[2] = false;
    }
}

// ---------------------------------------------------------------- HELP pages
void Game::openHelp() {
    helpBack = state;
    state = ST_HELP;
    helpPage = 0;
}

void Game::updateHelp(float dt) {
    updateFx(dt);
    if (in.escPressed) state = helpBack;
    helpPage = std::max(0, std::min(HELP_PAGES - 1, helpPage + (in.wheel < 0 ? 1 : in.wheel > 0 ? -1 : 0)));
}

void Game::drawHelp() {
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 170));
    const int X = 40, Y = 8, W = 320, H = 208;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, UI_BG);
    static const char* TITLES[HELP_PAGES] = {"CONTROLS", "HUNTING", "GATHERING AND CRAFTING", "TOWN, FARM AND TRAVEL", "THE PACK"};
    text(X + 8, Y + 7, std::string("HELP - ") + TITLES[helpPage] + "   " + S(helpPage + 1) + "/" + S(HELP_PAGES), C_GOLD);
    std::vector<std::string> L;
    auto k = [&](int a) { return keyName(a); };
    switch (helpPage) {
        case 0:
            L = {"MOVE: " + k(K_UP) + " " + k(K_LEFT) + " " + k(K_DOWN) + " " + k(K_RIGHT) + "  (ARROWS TOO)      SPRINT: " + k(K_SPRINT),
                 "INTERACT / HOLD TO GATHER: " + k(K_USE),
                 "HOLD LEFT CLICK TO CHARGE A THROW OR DRAW A BOW, RELEASE TO FIRE",
                 "1-8 OR WHEEL: HOTBAR     " + k(K_HEAL) + ": BANDAGE     " + k(K_MOUNT) + ": MOUNT     " + k(K_COMMAND) + ": COMPANION ORDER     " + k(K_DODGE) + ": DODGE ROLL",
                 "RIGHT CLICK OR " + k(K_MELEE) + ": MELEE SWING (TAP), HEAVY SWING (HOLD) - 3 QUICK SWINGS MAKE A COMBO",
                 k(K_PACK) + ": PACK     " + k(K_MAP) + ": MAP     TAB: MINIMAP     " + k(K_HELP) + ": HELP CARD",
                 k(K_BUILD) + ": BUILD MENU     " + k(K_ROTATE) + ": ROTATE     " + k(K_DEMOLISH) + ": DEMOLISH",
                 "ESC: PAUSE (SETTINGS, HELP, SAVE)     F11: FULLSCREEN",
                 "CHANGE ANY KEY IN SETTINGS."};
            break;
        case 1:
            L = {"ANIMALS FLEE, FIGHT OR STALK DEPENDING ON THEIR KIND.",
                 "A WOUNDED ANIMAL BLEEDS: IT LEAVES A BLOOD TRAIL AND LOSES HEALTH.",
                 "FOLLOW THE TRAIL - A KILL FROM BLEEDING STILL COUNTS FOR YOU.",
                 "AGGRESSIVE ANIMALS GIVE UP A CHASE AFTER A WHILE OR A DISTANCE.",
                 "ARROWS AND BOLTS ARE USED UP WHEN THEY HIT - A MISS CAN BE PICKED UP.",
                 "HOLD " + k(K_USE) + " AT A CARCASS TO TAKE HIDE, MEAT AND MORE.",
                 "THE BESTIARY (PACK) REWARDS HUNTING EACH SPECIES. THE JOURNAL LISTS YOUR GOALS."};
            break;
        case 2:
            L = {"HOLD " + k(K_USE) + " AT TREES, ROCKS AND PLANTS: AXE FOR TREES, PICKAXE FOR ORE.",
                 "CRAFT IN THE PACK (CRAFTING TAB): MAX CRAFTS AS MANY AS YOU CAN.",
                 "CRAFTED THINGS ROLL A QUALITY: NORMAL, GOOD, OUTSTANDING,",
                 "EXCELLENT OR MASTERPIECE. BETTER QUALITY: BETTER STATS AND PRICE.",
                 "YOU CAN CARRY UP TO 150% OF YOUR LIMIT, BUT YOU MOVE SLOWER.",
                 "SELL AT THE SHOP: SELL 1, ... FOR AN AMOUNT SLIDER, OR ALL."};
            break;
        case 3:
            L = {"THE FOUR GATES OUT OF THE HOME FOREST START SEALED:",
                 "DESERT, THEN SWAMP, SNOW AND MOUNTAIN, IN THAT ORDER.",
                 "WALK INTO A GATE TO SEE ITS OFFERING - THE MAP (" + k(K_MAP) + ") LISTS THEM TOO.",
                 "BUILD ON THE HOMESTEAD: " + k(K_BUILD) + " BUILD MENU, FARM TOOLS ON THE HOTBAR.",
                 "THE ELDER GIVES QUESTS - THE SHOP BUYS AND SELLS."};
            break;
        default:
            L = {"CLICK A STACK TO SELECT IT - THE INFO CARD SHOWS ITS ACTION BUTTON.",
                 "DRAG A STACK ONTO ANOTHER SLOT TO MOVE, MERGE OR SWAP IT,",
                 "OR ONTO A HOTBAR SLOT TO EQUIP IT.",
                 "DOUBLE CLICK OR RIGHT CLICK: USE / EQUIP.   SHIFT + CLICK: THE SAME.",
                 "HOVER A STACK AND PRESS 1-8 TO PUT IT ON THAT HOTBAR SLOT.",
                 "THE SLIDER SETS HOW MANY - SPLIT MAKES A NEW STACK, DROP DROPS THEM."};
            break;
    }
    for (size_t i = 0; i < L.size(); i++) text(X + 10, Y + 26 + (int)i * 14, L[i], C_TEXT);
    if (button(X + 10, Y + H - 22, 50, 14, "PREV", helpPage > 0, HEX(0x4a3a2a))) helpPage--;
    if (button(X + 66, Y + H - 22, 50, 14, "NEXT", helpPage < HELP_PAGES - 1, HEX(0x4a3a2a))) helpPage++;
    if (button(X + W - 70, Y + H - 22, 60, 14, "BACK", true, HEX(0x5d7a2e))) state = helpBack;
}
