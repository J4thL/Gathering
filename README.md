# WILDWOOD HUNTER
Top-down pixel-art hunting game in C++17 + SDL2. All sprites, animations and sounds are generated in code - no asset files.

## Build
Linux:   sudo apt install libsdl2-dev cmake g++   (macOS: brew install sdl2 | Windows: vcpkg install sdl2)
         cmake -S . -B build && cmake --build build && ./build/wildwood
Or:      g++ -std=c++17 -O2 -Isrc $(sdl2-config --cflags) src/main.cpp src/*/*.cpp -o wildwood $(sdl2-config --libs)

## Project layout
Includes are module-relative (`#include "items/items.hpp"`, `src` is the include root). Most art and data modules are header-only on purpose.
```
src/main.cpp      entry point, command line modes (--headless, --shots, test suites)
src/core/         Game state (game.hpp); simulation split into game_logic (update loop), game_player, game_animals, game_inventory, game_save;
                  drawing split into game_render (primitives, menus), game_render_world, game_hud, game_panels; gfx, audio, data, Windows console
src/player/       hunter look and poses, skills, equipment and hotbar logic
src/items/        item registry and the 100-slot inventory
src/crafting/     recipes and crafting
src/gathering/    resource nodes, gathering and harvesting
src/world/        maps, world generation, spawns, interiors, flowers
src/animals/      species table, animal animation, 3D-rendered animal art
src/combat/       monsters and their art
src/delve/        dungeon delves
src/farming/      homestead: plots, crops, farm animals, build menu, rendering
src/buildings/    structures and interior art
src/npcs/         town, shops and services, quests, hairdresser, villager art
src/art/          hunter / biome / gear / resource / trophy sprites and the sprite exporter
src/tests/        headless test suites (run with ctest); the --selftest suite is split by topic into selftest_*.cpp
```

## Controls
WASD move | SHIFT sprint | mouse aim | HOLD LMB charge, release to throw | 1-8 hotbar slot (weapon, tool, seeds or item in hand) / wheel cycles the hotbar | click uses the item in hand
E (hold) gather a carcass, chop a tree, mine a rock, pick a plant | E on a chest opens it, hold E on your campfire to pick it up | E at merchant: shop | E at Elder Holt: quests | Q bandage | M full map | TAB minimap | H help | ESC pause | F11 fullscreen

## Loop
Hunt rabbits, deer, boars and wolves -> pick thrown weapons back up -> hold E on the carcass with your skinning knife
to gather meat, hide, bone, organs, fur, tusk, antler -> sell at the Trading Post or craft weapons / upgrade
knife, backpack, boots and armor. Boars and wolves fight back; wolves get bolder at night. Progress autosaves.

## Quests
Elder Holt, by the campfire, hands out one quest at a time (a gold "!" marks him; a green "?" means turn it in).
Eight story quests (kill, gather, craft, earn) lead into endless scaling bounties. Rewards are coins, materials and
bandages. The active quest shows bottom-left, and the elder is marked on the minimap. Quest progress is saved.

## World
The game has five separate maps, each exactly 240x168 tiles: WILDWOOD (the start map: the original 88x60 Wildwood area in the centre of
home forest, with four gates at the map edge; there is no wall round the original area, its forest simply continues into the wider forest), DESERT, SWAMP, SNOW and MOUNTAIN. Each biome map has its own terrain, props, weather, landmarks
(walk up to one to discover it), gathering plants/ore, wildlife table and a trail from its gate to its hub landmark. Walk out of
WILDWOOD through a gate (east desert, south swamp, north snow, west mountain) to travel; each biome has one gate leading back home. A
"TO <MAP>" label shows near a gate and the minimap marks gates in the colour of their destination. Travelling keeps your health, pack,
skills, quests and the time of day; each map remembers its own campfires and chests, harvested resources, discovered landmarks and dropped
weapons (only the map you are in is loaded; the others are rebuilt from the world seed when you enter them). Saves record the map you are in.
Terrain and climate matter: sand, mud and deep snow slow you down, ice makes you slide, heat/cold/thin air change how fast stamina drains
(the fur coat cancels the cold).
Maps are defined in `src/world/maps.hpp` (gates, entry and hub per map), generated in `src/world/world.hpp` / `src/world/worldgen.hpp`, and travel
is `Game::travelTo` in `src/world/maps.cpp`. To add a biome: a Biome id, a row in `MAPS`, a ground / landmark / resource section in
`worldgen.hpp` and a table in `spawns.hpp`. Saves from before the separate maps still load: the original area sits at the same tiles as before, and campfires / chests that stood out in the old biome areas are returned to your pack with their contents.

## New worlds and sun shadows
Every new game builds a new random world (the seed is kept in the save, so Continue returns to the same one; the pause menu shows it). A seed is only used when all five maps come out reachable (`src/world/worldfix.hpp` opens a way to anything cut off). Starting a new game with a save on disk asks first. Trees, rocks and buildings cast the shape of their own sprite as a sun shadow (`game_light.cpp`), ground tiles blend softly into each other (`src/art/tile_edges.hpp`), and meadow flowers grow in small clusters.

The home forest's floor comes from two noise fields: **density** (the old tree-density field) and a new **moisture** field (`homeFloor()` in `src/world/worldgen.hpp`).
Dense and wet gives moss, dense and dry gives pine needles, dense in between stays dark grass (the very densest dry cores are root-tangled earth), the woodland edge is leaf litter when dry and moss when wet, and the rest is meadow. Lake shores are mossy. Trees follow the floor (pines on needles and roots, oaks on leaf litter), and decor does too. The tiles are painted in `src/art/sprites_forest.hpp` and blend into their neighbours like every other ground. `wildwood --forest DIR` writes a close-up screenshot of each floor and of each kind of glade.

**Streams and groves:** about nine brooks wander from springs in the wet forest to the nearest lake (`T_SHALLOW`: clear shallow water you can wade through at reduced speed; a path that meets one is a ford, and a brook only exists if it reaches a lake). Their banks are damp moss, with lilies floating in them. Trees gather in groves (a clumping noise leaves open ground between stands, and a second one makes a stand mostly pines or mostly oaks); in open country a tree keeps its distance from the next; and the forest has old stumps (decor), fallen logs and dead trees, thickest in the root-tangled and mossy ground.

**Flower fields:** three per world, wide open meadows (9-13 tiles across) in open country, away from the village, lakes, glades and the forest. No trees grow in them; they are carpeted with flowers, thickest in the middle, in drifts of one colour (orange, cream, violet `D_VIOLET`, poppy red `D_POPPY`: the last two are new flowers used only here), with a few herbs to gather. Rabbits and deer favour them, boars and wolves keep off. Chosen in `worldgen.hpp`, listed in `World::fields`, switched off with `GEN_NO_FIELDS`.

**Smoothing and independent layers:** a cleanup pass takes single-tile specks and one-tile spurs of one floor over to the floor around them (water banks, glades and the village are left alone). Every home-forest tile draws from a random stream of its own, seeded from its position, so a layer can be left out (`World::genOff`: `GEN_NO_STREAMS`, `GEN_NO_GLADES`, `GEN_NO_SMOOTH`) without moving any tree it does not touch (a self-test checks it). `wildwood --forest DIR` also writes `density.ppm` and `moisture.ppm`, the two fields behind the floor, for tuning.

**Animal routines and senses (home forest):** each of the four species lives by a daily routine (`chooseAct` / `performAct` in `game_animals.cpp`). It has a bed (a sheltered spot on the ground its species sleeps on, never in a glade or flower field) and, for deer, a watering place, and its day is made of acts: sleep, graze, drink, forage, patrol, roam. Rabbits sleep in burrows all night; deer drink and graze at dawn and dusk and lie up in the thicket at midday; boars root about at all hours and wallow at midday; wolves sleep by their den by day, go back to it at dawn and make their rounds at dusk and night. A surplus animal (the hour calls for fewer) walks to its bed and is gone once it is there and out of sight, not mid-meadow.
What an animal notices is now decided by `Game::sightFactor` instead of distance alone: its **field of view** (prey see almost all round, with a blind arc behind; predators look further ahead; outside the view it only hears), **cover** (each tree, rock or wall between it and the hunter cuts the range: one to 60%, two to 35%, three or more hide the hunter entirely), **light** (prey see worse at dusk and night), **sleep** (a sleeping animal is slow to notice) and **noise** (a sprint is heard through trees from about half the range; a hunter standing still is noticed later). Predators and prey use the same rules on each other. **F3** shows the AI overlay: every animal's act and state, its route (yellow) and bed (blue), its field of view (cyan) and the line of sight to you (green clear, orange screened, red seen) with the percentage of its range it keeps. `wildwood --forest DIR` writes `ai_debug.bmp`.

**Wolf packs:** wolves live in packs of 3-5 with one leader (`spawnPack`, `packLeader` in `game_animals.cpp`); they spawn and return as packs, with a lone wolf only as a leftover. The followers keep to the leader (they take its act and go to its place, spread by slot, and sleep beside it), catch up if they lag, and if the leader is lost another is promoted. When one wolf sees the hunter the whole pack within ~500 px joins the chase and the one that saw you gives a howl; the followers **flank**, circling to either side of the hunter before closing in, so a pack comes at you from several directions. A pack wolf breaks off at 40% health (a lone wolf at 25%), a hurt wolf goes back to its bed and slowly heals asleep, and if the **leader falls the pack scatters** for a few seconds. As the evening comes (dusk) the packs **howl** (`S_HOWL`, synthesised in `audio.hpp`, quieter the further the pack is), so the first night is announced before it arrives. In the F3 overlay a pack member shows as `wolf#N` (the leader `#NL`) with a line to its leader.
(Overlay colours: blue = bed, yellow = route, cyan = view, purple = to the leader, line to you green = clear / orange = screened / red = seen.)

**Wildlife by hour and ground:** the home forest's four animals each have a population that follows the clock (`Game::homeTarget`): rabbits are many by day and few at night, deer are busiest at dawn and dusk, boars are about at every hour (a few more at night), and wolves hunt at night (about 2.5x as many as at noon). When the hour calls for fewer, a surplus animal quietly leaves from out of sight (`Game::cullHome`; never rare or legendary ones, never one in a fight). Placement follows the ground (`zoneWeight` in `game_animals.cpp`): rabbits and deer graze meadows, leaf litter and glades (deer also like water), boars favour thickets, moss and roots, wolves keep to needles, roots and dark thickets by day and range over the meadows at night. Boars and wolves shy away from glades.

**Glades:** every world has about 14 open clearings cut out of the dense forest (away from the village, lakes and each other), with the trees thinning at their rim and flowers drifting across the meadow inside. Each has a feature: a flower-carpeted meadow, one great old oak, a rune stone that glows at night, a fairy ring of mushrooms with mushrooms to pick, a herb and flax patch, or a hunter's rest (a ring of stones and two logs). They are chosen in `worldgen.hpp` and listed in `World::glades`.

## Settings, help and shortcuts
The pause menu (Esc) and the title screen open SETTINGS: sound effects and ambience volume, a mute switch, and every key action can be rebound (click one, press a key;
a key already in use swaps with it; 1-8, arrows, Tab, Enter, Space, Esc and F11 are fixed). They are kept in `settings.txt` beside the save file. HELP (pause menu) has
five pages of controls and tips, written with your current keys. In the pack: double click or shift + click a stack to use / equip it (as right click does), and
hover a stack and press 1-8 to put it on that hotbar slot. The map (M) has a GATES list: click a gate to see what it asks for.

## Sealed gates
The four gates out of WILDWOOD start sealed and open one after the other: desert (east), then swamp (south), snow (north), mountain (west). Walk into
a sealed gate and it shows what it asks for: a hunting level and an offering of materials (for example wood, stone and hide for the desert, bars
and rope for the later ones). Press MAKE THE OFFERING (or Enter) to hand them over; the gate then stays open for good. A gate also needs the one before it
in the chain. Labels and the map show "(SEALED)" until then. The requirements are one table in `src/world/gates.hpp`. Saves record the open gates
as a `gates` line; a save from before gates existed keeps every gate open.

## Town economy and hunting depth
- **Reputation** (`src/core/game_economy.cpp`): STRANGER, KNOWN (25), TRUSTED (75), HONORED (160). Each level pays +4% when you sell and takes 5% off store goods. Points come from the elder's quests (+10), filled orders (+6 to +14) and every 150 coins you sell.
- **Market demand**: each material has a demand multiplier (0.7 to 1.3). Selling lowers it by 1% per item, and each new day it drifts back toward normal with a small random push. In the SELL tab the price is green when the store wants it and red when it has too much.
- **Orders**: the ORDERS tab of the general store lists three crafted items (things you can make at your skill level, never carried gear). Higher reputation asks for better minimum quality and pays 1.5 times the item's value. Orders are renewed every day (300 s of game time).
- **Bestiary card**: hover a hunted species in the BESTIARY tab to see its health, whether it fights back, which rarity tiers you have hunted and its drops (form and legendary extras are tagged).
- **Wounded quarry**: if an animal you hurt bleeds out beyond 150 px, its carcass blinks with a gold ring on the minimap and the map until you reach it.
- Saved lines: `rep`, `demand`, `order`, and `bestiary <kind> <kills> <tier mask>`. Older saves load with defaults.

## Wildlife
28 species (`src/animals/fauna.hpp` is the table: stats, drops, behaviour flags, rarity looks). Home keeps its original herds; the four biome maps
stream species in just off-screen (`src/world/spawns.hpp`: per-biome caps, cooldowns, terrain, night caps, 60-animal ceiling).
Any specimen can roll a rarity tier: uncommon 15%, rare 4%, legendary 0.3% (max one alive, 5 min cooldown). Higher tiers are
recoloured, larger, tougher and drop more; rare and legendary pay a coin bounty. Rattlesnakes, panthers etc. are ambush / night hunters.

## Items and ranged weapons
`src/items/items.hpp` is the item registry: one id space and one table (name, kind, stack size, price, backpack slots) for materials,
thrown weapons, launchers, ammo, consumables and backpacks, built from the tables in `data.hpp`.
The hunting bow (hold to draw, 0.85 s for a full shot; damage and range scale with the draw) fires arrows; the crossbow fires
bolts the moment you click and then reloads for 1.6 s. Launchers are not used up; ammo is, though a share of the arrows and bolts that
hit can be picked up again. Both weapons and their ammo are sold on the shop's BUY tab (second page). Feathers, scales, horns and claws
drop from the new species (see `fauna.hpp`).

## Inventory
The pack (`src/items/inventory.hpp`) always has exactly 100 slots; every stackable item stacks to 999, launchers / tools / bags / carried gear take a
slot each, and different items never share a slot. What limits the hunter is **weight**: every item has a weight in kg, defined once in
`src/items/items.hpp` (`matWeight`, `wpnWeight`, ...), and the pack has a maximum carrying weight. `Inventory::add` never takes more than fits (slots
or weight), so gathering, looting, pickups, crafting, shop purchases and chest withdrawals all refuse politely ("PACK FULL" / "TOO HEAVY")
and nothing is ever deleted. Carrying limit: 50 kg with no bag; bags are crafted items you wear (HIDE SATCHEL 100, TRAPPER PACK 150,
EXPLORER PACK 200, EXPEDITION PACK 300 kg). A bag only raises the limit - never the slots or stack size - and a worn bag has no weight.
There is no movement penalty for weight: being over the limit (e.g. after swapping to a smaller bag) just blocks new pickups until you
drop, sell, store or use something. Saves are version 2 (`slot <item name> <count>` lines, `bag` and `hotbar` lines; weights and limits are
never stored); version 1 saves are migrated on load (the old backpack level becomes the worn bag, owned weapons and consumables are
equipped once) and the original is kept as `save.v1.bak`.

## Carry weight and splitting stacks

You can carry up to 150% of your carry limit (the bag sets it): above 100% the hunter walks slower, evenly down to half speed at 150%, and
the HUD says SLOW; past 150% pickups are refused ("TOO HEAVY"). In the pack, **DROP..** opens an amount slider (drag the bar, -/+, 1/2, MAX,
or A/D and the mouse wheel; Enter confirms, Esc cancels) to drop part of a stack. In a storage chest, right-click an item to choose how many
to move (a normal click still moves the whole stack).

## Item quality

Everything you craft (weapons, ammunition, tools, bandages and other consumables, bags, placeables, carried gear, armor and boots) rolls a
quality when it is made - one roll per craft: **Normal** (stone border), **Good** (iron), **Outstanding** (bronze), **Excellent** (silver) or
**Masterpiece** (gold). The recipe's skill level raises the odds of every better tier (level 1 is mostly Normal; level 100 gives about a
48% chance of Good or better and 6% for a Masterpiece). Raw and processed materials never have a quality.
The border shows on icons in the pack, chests, hotbars and the HUD. Better quality means stats x1.08 / 1.17 / 1.28 / 1.45 (thrown and
launcher damage - ammunition counts half -, tool swing speed, armor protection, boot speed, bag capacity, healing) and sell price x1.3 / 1.7 / 2.3 / 3.5.
The general store's SELL tab also buys crafted things (one row per item and quality) for what their ingredients cost plus a fifth, times the quality price; boss drops with no recipe are not bought, and the best tool of a kind is always kept.
Stacks only merge at equal quality; the finest piece is used first, the lowest quality is spent first when ingredients or drops are removed.
Saves stay compatible: a quality is written only when it is not Normal (`slot <item> <count> <quality>`, `wornq` line) and old saves load as Normal.

## Pack screen and crafting
The main screen has no material counters any more (materials live in the pack). The hotbar at the bottom shows only what you have equipped:
keys `1`-`8` select a weapon or use the equipped consumable / place the structure, the wheel cycles equipped weapons, `Q` uses the lightest
equipped healing item. Nothing is auto-equipped except the starting knife and bandage.

`I` opens the pack (ESC or `I` closes; the world is paused). BACKPACK tab: the 10 x 10 slot grid, a carrying-weight bar ("85 / 150 KG"),
the worn bag (TAKE OFF), details of the selected item (quantity, weight each / per stack, attributes), EQUIP / USE / WEAR (ENTER), DROP 1 /
DROP ALL (the stack lies on the ground until you walk over it again), TO HOTBAR and SORT. Move with the arrow keys / WASD or click a slot;
`TAB` switches tabs. The HOTBAR row at the bottom shows the 8 quick slots (3 weapon, 2 healing, food, buff, utility): click a slot to equip the
selected item there or to unequip what is in it.
CRAFTING tab: every recipe with category filters (tools, weapons, ammo, medicine, survival, parts, gear), a CAN MAKE switch, have/need
counts per ingredient (red when short), a tooltip with the reason a recipe is blocked, and a scroll wheel. Recipes live in `src/crafting/recipes.hpp`.
Stations: ANYWHERE, FIRE (the village fire or a campfire you place) and SHOP (beside the merchant).
Every recipe names a skill and a minimum level (`skill`, `minLv` in `src/crafting/recipes.hpp`); a locked recipe shows "NEEDS <SKILL> <lv>" and costs nothing.
Ingredients are spent only when the result is delivered. Processing: planks, bone fragments, leather, grease (from carcass fat) and extracts feed
most of the newer recipes. Every animal drops one exclusive trophy material (`M_TROPHY0 + species` in `src/core/data.hpp`) that some recipe uses.
Crafted gear: fang dagger, reinforced spear, recurve bow, heavy axe, four biome weapons (+30% damage in their biome), armor tiers up to bear hide,
swiftfoot boots, the bags (satchel to expedition pack) and carried gear (wolf cloak, parka, desert wraps, venom wraps, waders, climbing harness, quiver)
that works while it is in the pack. New medicines and foods: poultice, herbal medicine, dried meat, hunter's stew, velvet tonic, nightsight tonic.

## Skills
The SKILLS tab of the pack lists 16 skills (levels 1-100; data and curve in `src/player/skills.hpp`, tuning constants `XP_BASE`, `XP_EXP`, `MILESTONES`).
XP is awarded only after an action really completes: a finished resource (Woodcutting / Mining / Foraging), a kill (Hunting and Ranged,
reduced when you grind animals far below your level), each carcass piece (Skinning / Butchering), a delivered craft (the recipe's skill), a hit
survived in armor (Defense), a new landmark or a used consumable (Survival). Each skill has one capped bonus (shown when you hover it): chop /
mine speed, extra plants, weapon damage, extra pelts / meat, a chance to save a craft, keeping ammo, damage reduction, stronger healing. A bonus
grows evenly to 60% of its cap at level 100 and gains 10% of the cap at each milestone (10 / 25 / 50 / 75). Progress is saved as `skill <key> <xp>`;
saves without skills load at level 1.
**Perks:** each milestone opens a one-time choice per skill (click the skill row; a "!" marks skills with a pending choice). A is MASTERY (adds
8 / 10 / 12 / 15% of the skill's cap to its bonus); B is extra XP for that skill (10 / 15 / 20 / 30%), or extra speed for woodcutting and mining.
A choice is final and saved as `perk <key> <tier> <0|1>`.
**Hunter rank:** the sum of all skill levels (max 1600) sets your rank: Apprentice, Tracker (60), Warden (250), Master (600), Grandmaster (1000). Every rank adds 10 kg
to the carry limit, and Warden and up are offered one quality step better on the orders board. It is derived from the skills, so nothing extra is saved.
**Trophies:** the BESTIARY tab's TROPHIES button lists nine milestones read from the bestiary (10 / 20 / all species found, 1 / 5 / 10 legendary animals hunted, 100 / 500 / 2000 total
kills). Each pays coins once when reached (saved as `trophy <mask>`); older saves earn the ones they already qualify for on the next kill.
**Journal:** the pack's last tab collects the active quest, today's hunt trial, the three store orders and the next trophy, rank and perk goals on one page (read only).
**Trophy wall:** a 4 x 1 outdoor piece in the Homestead's build menu (6 planks, 1 rope). It shows a plaque for each of the nine trophies (rows: species, legends, kills; columns: bronze, silver, gold), lit once earned.
**Frostbound King, again:** he still returns 30 minutes after he falls, but each return is stronger (+40% health and +25% damage per tier, up to tier 4). Killing a stronger king pays 300 coins and 500 hunting XP per tier on top of his chest. The tier is saved as `kingtier <n>` and shown in the journal.
**Seasons:** every 7 days (a day is 5 minutes) the season changes: spring, summer, autumn, winter, shown beside the day on the clock. Spring has 25% more animals and more storms, summer fewer storms, autumn gives one extra meat and fat from a kill, winter has 25% fewer animals but one extra fur and hide. Each season also tints the light faintly. Nothing is saved for it: the season comes from the game clock.
**Fishing:** craft a FISHING ROD (2 branches, 3 fibre, 1 bone; Crafting tab), take it in hand from the hotbar, aim at any water within about 8 tiles and click to cast. The bobber dips after a few seconds: click again within a second to land the fish (clicking early reels in empty, too late lets it go). Six fish (trout, bass, catfish, eel, arctic char, pike) bite depending on the water's biome and the season. The FISHING skill (16th row) shortens the wait, gives a chance of a second fish per catch and takes the usual perks; three trophies (10 / 50 / 200 fish) pay coins and show on the trophy wall; with a rod in the pack, every fourth day's hunt trial is a fishing goal. Fish caught are saved as `fish <n>`.
**Companion:** in the Homestead's animal panel a grown mount (a raised wild animal) has a COMPANION button (the one that replaces COLLECT). Outside the homestead the companion walks behind you; nothing hunts it and nothing you throw can hit it. Every kill you make with it close by adds to its bond (levels at 25 / 80 / 200 / 450 kills); each level adds 2% weapon damage (2% at the first level up to 10% at the fifth), and every couple of minutes (faster with a deeper bond) it digs up a small find (herb, berry, mushroom, fibre, branch, and from level 4 sometimes amber). It goes back to its pen when you do, and cannot be chosen while you ride a mount. Saved with the homestead animal as `companion bond` fields; older saves load without one.
**Daily hunt trial:** shown on the BESTIARY tab. Each day sets one kill goal: a number (3 + 2 per hunter rank, plus 0-2) of a species you have already found, or, for Warden and up on every third day, one legendary animal.
Finishing it pays coins and a little reputation once. The day's trial and its progress are saved as `trial <kind> <species> <need> <got> <pay> <done>`.

## Gathering
Trees, rocks, ore and plants are harvested by holding E beside them (data in `src/gathering/resources.hpp`, behaviour in `src/gathering/gathering.cpp`).
Tools come in four tiers (stone, copper, iron, gilded): higher tiers swing faster, give bonus yield and open harder resources
(copper and iron ore need a tier 2 pickaxe, gold and crystal tier 3). Each resource takes a number of hits, shows a hit counter and progress
bar, drops its yields, then becomes a stump / rubble / stub and regrows after a respawn time (trees become solid again). A resource is never
finished into a full bag, and nothing can be gathered through walls. Trees on the map's edge wall are never harvestable.
Each biome has its own plants and ore; thorny and poisonous plants hurt unless you wear armor. Progression: stone tools -> campfire,
rope and copper bars -> copper tools -> iron bars at the shop -> iron and gilded tools, crossbow, scale mail. Campfires and chests are crafted
and placed from the pack; chests hold 20 slots.

## The town
The middle of Wildwood is a town: cobbled streets, lamps, fences, a plaza with the elder, a well and the campfire. Six buildings, each with a
sign and its own interior map (walk up to the door, **E** to enter, **E** on the doormat to leave; you come back out on the exact spot you went in from):
- **Blacksmith** (Brokk): smelting ore into ingots, metal tools, weapons and gear; the tool upgrade (coins) is sold here.
- **Leatherworks** (Hild): hide into leather, armor, boots, bags, carried gear; the armor / boots upgrades are sold here.
- **Workshop** (Tobin): everything else you craft (wood and bone weapons, ammo, medicine, food, campfires, chests, bouquets).
- **General Store** (Garrick): buying and selling only.
- **Flower Shop** (Mary Rose): buy cut flowers (rose, tulip, lavender, sunflower), sell flowers and bouquets; tie bouquets at the workshop.
- **Salon** (Salona Mia): hairstyle (5), hair colour (8), skin (5), tunic colour (6) with a live preview; CONFIRM costs 25 coins and redraws the hunter, CANCEL costs nothing.
All crafting happens inside the matching building (the pack's CRAFTING tab is now a recipe book that says where each recipe is made). The look is saved (`look` line).
Code: `town.hpp` (table), `sprites_town.hpp` / `sprites_interior.hpp` (art), `interiors.hpp` (room layouts), `town.cpp` (doors), `services.cpp`, `flowers.cpp`, `salon.cpp`, `appearance.hpp`.

## Test flags
    wildwood --headless --worldcheck      per map: size, biome purity, reachability of every hub/landmark/plant/gate, walks through every gate and back, terrain/climate checks
    wildwood --headless --tour DIR        screenshots of every map's entrance, hub, landmarks and gates (BMP)
- `--town DIR` - enters every town building, checks reachability, collision, exact return, services (crafting rules, flowers, salon, save of the look) and writes screenshots
    wildwood --headless --dumpcore FILE   dump of the original home area, for diffing against an older build
    wildwood --headless --shots DIR       the original scripted playthrough
    wildwood --headless --wildlife DIR    species sheets, tier odds, drops, behaviour probes, 10-minute spawn sims per biome
    wildwood --headless --weapons         item registry checks, bow / crossbow / ammo behaviour, drops, save round trip
    wildwood --headless --selftest        inventory class, weight rules, bags, hotbar, full-pack rules, pack screen + crafting, gathering, tools, respawn, recipes, campfires and chests, save v2 and v1 migration, separate maps (travel, per-map state, save / load, old-save conversion)
