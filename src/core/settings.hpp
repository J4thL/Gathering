// settings.hpp - the player's settings: sound volumes and the rebindable keys. Kept in settings.txt next to the save file
// (not in the save: it belongs to the player, not to a hunter). Hotbar digits, Esc, Tab, Enter / Space, the arrow keys and F11 are fixed.
#pragma once
#include <SDL.h>

enum KeyAct { K_UP, K_DOWN, K_LEFT, K_RIGHT, K_SPRINT, K_USE, K_HEAL, K_MAP, K_HELP, K_PACK, K_BUILD, K_ROTATE, K_DEMOLISH, K_COUNT };

struct KeyInfo {
    const char* label;
    SDL_Scancode def;
};
static const KeyInfo KEYINFO[K_COUNT] = {
    {"MOVE UP", SDL_SCANCODE_W},   {"MOVE DOWN", SDL_SCANCODE_S}, {"MOVE LEFT", SDL_SCANCODE_A},     {"MOVE RIGHT", SDL_SCANCODE_D},
    {"SPRINT", SDL_SCANCODE_LSHIFT}, {"INTERACT", SDL_SCANCODE_E}, {"BANDAGE", SDL_SCANCODE_Q},       {"MAP", SDL_SCANCODE_M},
    {"HELP CARD", SDL_SCANCODE_H}, {"PACK", SDL_SCANCODE_I},      {"BUILD MENU", SDL_SCANCODE_B},    {"ROTATE", SDL_SCANCODE_R},
    {"DEMOLISH", SDL_SCANCODE_X},
};

// keys that cannot be given to an action
inline bool keyReserved(SDL_Scancode sc) {
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0) return true;  // hotbar
    switch (sc) {
        case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_TAB: case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE: case SDL_SCANCODE_F11:
        case SDL_SCANCODE_UP: case SDL_SCANCODE_DOWN: case SDL_SCANCODE_LEFT: case SDL_SCANCODE_RIGHT: case SDL_SCANCODE_UNKNOWN: return true;
        default: return false;
    }
}

struct Settings {
    int sfx = 10, amb = 10;  // volume steps 0..10
    SDL_Scancode key[K_COUNT];
    Settings() { resetKeys(); }
    void resetKeys() {
        for (int i = 0; i < K_COUNT; i++) key[i] = KEYINFO[i].def;
    }
};
