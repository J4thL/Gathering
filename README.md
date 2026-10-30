# WILDWOOD HUNTER
Top-down pixel-art hunting game in C++17 + SDL2. All sprites, animations and sounds are generated in code - no asset files.

## Build
Linux:   sudo apt install libsdl2-dev cmake g++   (macOS: brew install sdl2 | Windows: vcpkg install sdl2)
         cmake -S . -B build && cmake --build build && ./build/wildwood
Or:      g++ -std=c++17 -O2 $(sdl2-config --cflags) src/*.cpp -o wildwood $(sdl2-config --libs)

## Controls
WASD move | SHIFT sprint | mouse aim | HOLD LMB charge, release to throw | 1-4 / wheel weapon
E (hold) gather a carcass, chop a tree, mine a rock, pick a plant | E on a chest opens it, hold E on your campfire to pick it up | E at merchant: shop | E at Elder Holt: quests | Q bandage | M map | H help | ESC pause | F11 fullscreen

## Loop
Hunt rabbits, deer, boars and wolves -> pick thrown weapons back up -> hold E on the carcass with your skinning knife
to gather meat, hide, bone, organs, fur, tusk, antler -> sell at the Trading Post or craft weapons / upgrade
knife, backpack, boots and armor. Boars and wolves fight back; wolves get bolder at night. Progress autosaves.

## Quests
Elder Holt, by the campfire, hands out one quest at a time (a gold "!" marks him; a green "?" means turn it in).
Eight story quests (kill, gather, craft, earn) lead into endless scaling bounties. Rewards are coins, materials and
bandages. The active quest shows bottom-left, and the elder is marked on the minimap. Quest progress is saved.

## World
The game has five separate maps, each exactly 240x168 tiles: WILDWOOD (the start map: the original 88x60 Wildwood area, unchanged, in the centre of
home forest, with four gates at the map edge), DESERT, SWAMP, SNOW and MOUNTAIN. Each biome map has its own terrain, props, weather, landmarks
(walk up to one to discover it), gathering plants/ore, wildlife table and a trail from its gate to its hub landmark. Walk out of
WILDWOOD through a gate (east desert, south swamp, north snow, west mountain) to travel; each biome has one gate leading back home. A
"TO <MAP>" label shows near a gate and the minimap marks gates in the colour of their destination. Travelling keeps your health, pack,
skills, quests and the time of day; each map remembers its own campfires and chests, harvested resources, discovered landmarks and dropped
weapons (only the map you are in is loaded; the others are rebuilt from the world seed when you enter them). Saves record the map you are in.
Terrain and climate matter: sand, mud and deep snow slow you down, ice makes you slide, heat/cold/thin air change how fast stamina drains
(the fur coat cancels the cold).
Maps are defined in `src/maps.hpp` (gates, entry and hub per map), generated in `src/world.hpp` / `src/worldgen.hpp`, and travel
is `Game::travelTo` in `src/maps.cpp`. To add a biome: a Biome id, a row in `MAPS`, a ground / landmark / resource section in
`worldgen.hpp` and a table in `spawns.hpp`. Saves from before the separate maps still load: the original area sits at the same tiles as before, and campfires / chests that stood out in the old biome areas are returned to your pack with their contents.

## Wildlife
28 species (`src/fauna.hpp` is the table: stats, drops, behaviour flags, rarity looks). Home keeps its original herds; the four biome maps
stream species in just off-screen (`src/spawns.hpp`: per-biome caps, cooldowns, terrain, night caps, 60-animal ceiling).
Any specimen can roll a rarity tier: uncommon 15%, rare 4%, legendary 0.3% (max one alive, 5 min cooldown). Higher tiers are
recoloured, larger, tougher and drop more; rare and legendary pay a coin bounty. Rattlesnakes, panthers etc. are ambush / night hunters.

## Items and ranged weapons
`src/items.hpp` is the item registry: one id space and one table (name, kind, stack size, price, backpack slots) for materials,
thrown weapons, launchers, ammo, consumables and backpacks, built from the tables in `data.hpp`.
The hunting bow (hold to draw, 0.85 s for a full shot; damage and range scale with the draw) fires arrows; the crossbow fires
bolts the moment you click and then reloads for 1.6 s. Launchers are not used up; ammo is, though a share of the arrows and bolts that
hit can be picked up again. Both weapons and their ammo are sold on the shop's BUY tab (second page). Feathers, scales, horns and claws
drop from the new species (see `fauna.hpp`).

## Inventory
The pack is slot-based (`src/inventory.hpp`): every item stack takes one slot, materials / ammo stack to 99, thrown weapons to 30,
bandages to 9, launchers never stack. The backpack upgrade only adds slots: 20 -> 30 -> 40. A full bag refuses gathering, pickups,
crafting and purchases that do not fit an existing stack or a free slot. Saves are version 2 (`slot <item name> <count>` lines, written
to a temp file and renamed); version 1 saves are migrated on load and the original is kept as `save.v1.bak`.

## Pack screen and crafting
`I` opens the pack (ESC or `I` closes; the world is paused). BACKPACK tab: a grid of your slots with counts, details of the selected or
hovered item, EQUIP / USE (ENTER) and SORT (merges split stacks). Move with the arrow keys / WASD or click a slot; `TAB` switches tabs.
CRAFTING tab: every recipe with category filters (tools, weapons, ammo, medicine, survival, parts, gear), a CAN MAKE switch, have/need
counts per ingredient (red when short), a tooltip with the reason a recipe is blocked, and a scroll wheel. Recipes live in `src/recipes.hpp`.
Stations: ANYWHERE, FIRE (the village fire or a campfire you place) and SHOP (beside the merchant).
Every recipe names a skill and a minimum level (`skill`, `minLv` in `src/recipes.hpp`); a locked recipe shows "NEEDS <SKILL> <lv>" and costs nothing.
Ingredients are spent only when the result is delivered. Processing: planks, bone fragments, leather, grease (from carcass fat) and extracts feed
most of the newer recipes. Every animal drops one exclusive trophy material (`M_TROPHY0 + species` in `src/data.hpp`) that some recipe uses.
Crafted gear: fang dagger, reinforced spear, recurve bow, heavy axe, four biome weapons (+30% damage in their biome), armor tiers up to bear hide,
swiftfoot boots, the explorer pack (50 slots) and carried gear (wolf cloak, parka, desert wraps, venom wraps, waders, climbing harness, quiver)
that works while it is in the pack. New medicines and foods: poultice, herbal medicine, dried meat, hunter's stew, velvet tonic, nightsight tonic.

## Skills
The SKILLS tab of the pack lists 15 skills (levels 1-100; data and curve in `src/skills.hpp`, tuning constants `XP_BASE`, `XP_EXP`, `MILESTONES`).
XP is awarded only after an action really completes: a finished resource (Woodcutting / Mining / Foraging), a kill (Hunting and Ranged,
reduced when you grind animals far below your level), each carcass piece (Skinning / Butchering), a delivered craft (the recipe's skill), a hit
survived in armor (Defense), a new landmark or a used consumable (Survival). Each skill has one capped bonus (shown when you hover it): chop /
mine speed, extra plants, weapon damage, extra pelts / meat, a chance to save a craft, keeping ammo, damage reduction, stronger healing. A bonus
grows evenly to 60% of its cap at level 100 and gains 10% of the cap at each milestone (10 / 25 / 50 / 75). Progress is saved as `skill <key> <xp>`;
saves without skills load at level 1.

## Gathering
Trees, rocks, ore and plants are harvested by holding E beside them (data in `src/resources.hpp`, behaviour in `src/gathering.cpp`).
Tools come in four tiers (stone, copper, iron, gilded): higher tiers swing faster, give bonus yield and open harder resources
(copper and iron ore need a tier 2 pickaxe, gold and crystal tier 3). Each resource takes a number of hits, shows a hit counter and progress
bar, drops its yields, then becomes a stump / rubble / stub and regrows after a respawn time (trees become solid again). A resource is never
finished into a full bag, and nothing can be gathered through walls. Trees on the map border and around the home area are never harvestable.
Each biome has its own plants and ore; thorny and poisonous plants hurt unless you wear armor. Progression: stone tools -> campfire,
rope and copper bars -> copper tools -> iron bars at the shop -> iron and gilded tools, crossbow, scale mail. Campfires and chests are crafted
and placed from the pack; chests hold 20 slots.

## Test flags
    wildwood --headless --worldcheck      per map: size, biome purity, reachability of every hub/landmark/plant/gate, walks through every gate and back, terrain/climate checks
    wildwood --headless --tour DIR        screenshots of every map's entrance, hub, landmarks and gates (BMP)
    wildwood --headless --dumpcore FILE   dump of the original home area, for diffing against an older build
    wildwood --headless --shots DIR       the original scripted playthrough
    wildwood --headless --wildlife DIR    species sheets, tier odds, drops, behaviour probes, 10-minute spawn sims per biome
    wildwood --headless --weapons         item registry checks, bow / crossbow / ammo behaviour, drops, save round trip
    wildwood --headless --selftest        inventory class, full-bag rules, backpack upgrades, pack screen + crafting, gathering, tools, respawn, recipes, campfires and chests, save v2 and v1 migration, separate maps (travel, per-map state, save / load, old-save conversion)
