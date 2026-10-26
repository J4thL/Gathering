# WILDWOOD HUNTER
Top-down pixel-art hunting game in C++17 + SDL2. All sprites, animations and sounds are generated in code - no asset files.

## Build
Linux:   sudo apt install libsdl2-dev cmake g++   (macOS: brew install sdl2 | Windows: vcpkg install sdl2)
         cmake -S . -B build && cmake --build build && ./build/wildwood
Or:      g++ -std=c++17 -O2 $(sdl2-config --cflags) src/*.cpp -o wildwood $(sdl2-config --libs)

## Controls
WASD move | SHIFT sprint | mouse aim | HOLD LMB charge, release to throw | 1-4 / wheel weapon
E (hold) gather carcass | E at merchant: shop | E at Elder Holt: quests | Q bandage | M map | H help | ESC pause | F11 fullscreen

## Loop
Hunt rabbits, deer, boars and wolves -> pick thrown weapons back up -> hold E on the carcass with your skinning knife
to gather meat, hide, bone, organs, fur, tusk, antler -> sell at the Trading Post or craft weapons / upgrade
knife, backpack, boots and armor. Boars and wolves fight back; wolves get bolder at night. Progress autosaves.

## Quests
Elder Holt, by the campfire, hands out one quest at a time (a gold "!" marks him; a green "?" means turn it in).
Eight story quests (kill, gather, craft, earn) lead into endless scaling bounties. Rewards are coins, materials and
bandages. The active quest shows bottom-left, and the elder is marked on the minimap. Quest progress is saved.

## World
The map is 240x168 tiles. The original Wildwood sits unchanged in the centre; each of its four dirt paths now leads through
a gate in the tree wall to a new biome: SNOWFIELDS (north), DESERT (east), SWAMP (south), MOUNTAINS (west). Each has its own
terrain, props, weather, landmarks (walk up to one to discover it) and gathering plants/ore. Terrain and climate matter:
sand, mud and deep snow slow you down, ice makes you slide, heat/cold/thin air change how fast stamina drains
(the fur coat cancels the cold). The minimap shows discovered landmarks and the biome you are in.

## Wildlife
28 species (`src/fauna.hpp` is the table: stats, drops, behaviour flags, rarity looks). Home keeps its original herds; the four new biomes
stream species in just off-screen (`src/spawns.hpp`: per-biome caps, cooldowns, terrain, night caps, 60-animal ceiling).
Any specimen can roll a rarity tier: uncommon 15%, rare 4%, legendary 0.3% (max one alive, 5 min cooldown). Higher tiers are
recoloured, larger, tougher and drop more; rare and legendary pay a coin bounty. Rattlesnakes, panthers etc. are ambush / night hunters.

## Test flags
    wildwood --headless --worldcheck      reachability of every hub/landmark/plant + a scripted walk to each biome + terrain/climate checks
    wildwood --headless --tour DIR        screenshots of every biome, landmark and border gate (BMP)
    wildwood --headless --dumpcore FILE   dump of the original home area, for diffing against an older build
    wildwood --headless --shots DIR       the original scripted playthrough
    wildwood --headless --wildlife DIR    species sheets, tier odds, drops, behaviour probes, 10-minute spawn sims per biome
