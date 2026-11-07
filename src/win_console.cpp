// win_console.cpp - Windows only. A MinGW build links with -mwindows (no console window for the game), so `wildwood --selftest` run from the
// VS Code / PowerShell terminal printed nothing. Called first thing in main() when there are command-line flags: if the game was started
// from a terminal, its output goes there.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>

void attachParentConsole() {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return;  // already has a console (MSVC build) or was not started from a terminal
    FILE* f = nullptr;
    f = freopen("CONOUT$", "w", stdout);
    f = freopen("CONOUT$", "w", stderr);
    (void)f;
}
#endif
