// logic_util.hpp - small helpers shared by the game_*.cpp simulation files.
#pragma once
#include "core/game.hpp"

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float smoothf(float a, float b, float x) {
    float t = clampf((x - a) / (b - a), 0, 1);
    return t * t * (3 - 2 * t);
}

// move with axis-separated tile collision, in small steps so nothing tunnels
inline void moveEnt(const World& w, V2& pos, V2 d, float r) {
    int steps = (int)ceilf(std::max(fabsf(d.x), fabsf(d.y)) / 3.f);
    if (steps < 1) steps = 1;
    V2 s = d * (1.f / steps);
    for (int i = 0; i < steps; i++) {
        if (!w.blocked(pos.x + s.x, pos.y, r)) pos.x += s.x;
        if (!w.blocked(pos.x, pos.y + s.y, r)) pos.y += s.y;
    }
}

// the corrupt-save recovery (game_save.cpp): called by Game::init
void recoverSave(const std::string& path);
