cmake_minimum_required(VERSION 3.10)
project(WildwoodHunter CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# sources, grouped by module (see README.md for what each folder holds)
set(WILDWOOD_MAIN src/main.cpp)
set(WILDWOOD_CORE
    src/core/game_logic.cpp
    src/core/game_render.cpp)
set(WILDWOOD_PLAYER
    src/player/skills.cpp
    src/player/equipment.cpp)
set(WILDWOOD_CRAFTING
    src/crafting/crafting.cpp)
set(WILDWOOD_GATHERING
    src/gathering/gathering.cpp)
set(WILDWOOD_WORLD
    src/world/maps.cpp
    src/world/flowers.cpp)
set(WILDWOOD_COMBAT
    src/combat/monsters.cpp)
set(WILDWOOD_DELVE
    src/delve/delve.cpp
    src/delve/delve_render.cpp)
set(WILDWOOD_FARMING
    src/farming/homestead.cpp
    src/farming/homestead_ui.cpp
    src/farming/homestead_render.cpp)
set(WILDWOOD_BUILDINGS
    src/buildings/structures.cpp)
set(WILDWOOD_NPCS
    src/npcs/town.cpp
    src/npcs/services.cpp
    src/npcs/salon.cpp)
set(WILDWOOD_TESTS
    src/tests/selftest.cpp
    src/tests/delve_tests.cpp
    src/tests/homestead_tests.cpp
    src/tests/soak_tests.cpp
    src/tests/weapon_tests.cpp
    src/tests/world_check.cpp)

add_executable(wildwood ${WILDWOOD_MAIN} ${WILDWOOD_CORE} ${WILDWOOD_PLAYER} ${WILDWOOD_CRAFTING} ${WILDWOOD_GATHERING} ${WILDWOOD_WORLD} ${WILDWOOD_COMBAT} ${WILDWOOD_DELVE} ${WILDWOOD_FARMING} ${WILDWOOD_BUILDINGS} ${WILDWOOD_NPCS} ${WILDWOOD_TESTS})
target_include_directories(wildwood PRIVATE src)  # includes are written module-relative, e.g. #include "items/items.hpp"

if(WIN32)
  target_sources(wildwood PRIVATE src/core/win_console.cpp)
endif()

find_package(Threads REQUIRED)  # the 3D animal models are drawn on several threads at start-up
target_link_libraries(wildwood PRIVATE Threads::Threads)

find_package(SDL2 CONFIG QUIET)
if(TARGET SDL2::SDL2)
  # modern SDL2 config package (vcpkg, MSYS2, SDL2 dev zip, Linux distro packages)
  if(TARGET SDL2::SDL2main)
    target_link_libraries(wildwood PRIVATE SDL2::SDL2main SDL2::SDL2)
  else()
    target_link_libraries(wildwood PRIVATE SDL2::SDL2)
  endif()
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED sdl2)
  target_include_directories(wildwood PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(wildwood PRIVATE ${SDL2_LIBRARIES})
endif()

if(MSVC)
  target_compile_options(wildwood PRIVATE /W3 /utf-8)
  target_compile_definitions(wildwood PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
elseif(WIN32)
  target_link_libraries(wildwood PRIVATE -mwindows)
else()
  target_compile_options(wildwood PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# copy SDL2.dll next to the exe on Windows when available
if(WIN32 AND TARGET SDL2::SDL2)
  add_custom_command(TARGET wildwood POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:wildwood>
    VERBATIM)
endif()

# the headless test suites, runnable with `ctest` (or `ctest --output-on-failure`). They save into a temporary folder, never the player's real save.
enable_testing()
foreach(suite selftest weapons worldcheck delve homestead soak)
  add_test(NAME ${suite} COMMAND wildwood --headless --${suite})
  set_tests_properties(${suite} PROPERTIES TIMEOUT 900)
endforeach()
