#pragma once
// tool_modes.hpp - the screenshot / dump tool modes run from the command line (tool_modes.cpp).
#include <string>
#include "core/game.hpp"

void scriptedRun(Game& g, const std::string& dir);  // --shots: a scripted play-through with screenshots
void smithShots(Game& g, const std::string& dir);   // --smith
void townTour(Game& g, const std::string& dir);     // --town
void spriteSheet(Game& g, const std::string& dir);  // --sprites
void dumpCore(Game& g, const char* file);           // --dumpcore
void tour(Game& g, const std::string& dir);         // --tour: a screenshot of every map
void wildlife(Game& g, const std::string& dir);     // --wildlife
