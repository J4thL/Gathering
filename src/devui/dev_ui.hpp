// dev_ui.hpp - the developer panel (Dear ImGui). Press F1 to open it.
//
// It is a development tool, never part of the game: everything here compiles to empty inline stubs unless the build defines GATHERING_DEV_UI
// (CMake: -DWILDWOOD_DEV_UI=ON), so a normal build contains no ImGui code at all and behaves exactly as it always did.
#pragma once
#include <SDL.h>

struct Game;

#ifdef GATHERING_DEV_UI
namespace devui {
void init(Game& g);                              // after the renderer exists (not in headless runs)
void shutdown(Game& g);                          // before Game::shutdown destroys the renderer
bool processEvent(Game& g, const SDL_Event& e);  // true = the panel used the event, the game must not see it. F1 toggles the panel.
bool wantKeyboard();                             // a text box has focus: the game ignores the held movement keys
void draw(Game& g);                              // once per frame, after the game's canvas is on the window and before SDL_RenderPresent
}  // namespace devui
#else
namespace devui {
inline void init(Game&) {}
inline void shutdown(Game&) {}
inline bool processEvent(Game&, const SDL_Event&) { return false; }
inline bool wantKeyboard() { return false; }
inline void draw(Game&) {}
}  // namespace devui
#endif
