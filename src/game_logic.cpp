// game_logic.cpp - simulation: player, animals, projectiles, gathering, economy, save/load.
#include <cstdio>
#include <fstream>
#include <sstream>
#include "game.hpp"

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float smoothf(float a, float b, float x) {
    float t = clampf((x - a) / (b - a), 0, 1);
    return t * t * (3 - 2 * t);
}

// move with axis-separated tile collision, in small steps so nothing tunnels
static void moveEnt(const World& w, V2& pos, V2 d, float r) {
    int steps = (int)ceilf(std::max(fabsf(d.x), fabsf(d.y)) / 3.f);
    if (steps < 1) steps = 1;
    V2 s = d * (1.f / steps);
    for (int i = 0; i < steps; i++) {
        if (!w.blocked(pos.x + s.x, pos.y, r)) pos.x += s.x;
        if (!w.blocked(pos.x, pos.y + s.y, r)) pos.y += s.y;
    }
}

// ================================================================= platform bits
bool Game::init(bool headless) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    win = SDL_CreateWindow("WILDWOOD HUNTER", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1200, 675, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!win) {
        fprintf(stderr, "window failed: %s\n", SDL_GetError());
        return false;
    }
    if (!headless) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        fprintf(stderr, "renderer failed: %s\n", SDL_GetError());
        return false;
    }
    canvas = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, LW, LH);
    buildAssets(ren, A);
    world.generate(7);
    miniTex = toTex(ren, world.minimap());
    audio.init();
    SDL_ShowCursor(SDL_DISABLE);
    for (int i = 0; i < 30; i++) flies.push_back({rng.range(0, LW), rng.range(0, LH), rng.range(0, 6.28f), rng.range(0.6f, 1.4f)});
    std::ifstream t(savePath());
    hasSave = t.good();
    newGame();
    return true;
}

void Game::shutdown() {
    if (state != ST_TITLE) save();
    audio.shutdown();
    if (miniTex) SDL_DestroyTexture(miniTex);
    if (canvas) SDL_DestroyTexture(canvas);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
}

std::string Game::savePath() {
    char* b = SDL_GetPrefPath("WildwoodGames", "WildwoodHunter");
    std::string s = b ? std::string(b) + "save.txt" : std::string("wildwood_save.txt");
    if (b) SDL_free(b);
    return s;
}

void Game::save() {
    std::ofstream f(savePath());
    if (!f) return;
    f << "coins " << p.coins << "\nmats";
    for (int i = 0; i < M_COUNT; i++) f << ' ' << p.mats[i];
    f << "\nwpn";
    for (int i = 0; i < W_COUNT; i++) f << ' ' << p.wpn[i];
    f << "\nup";
    for (int i = 0; i < U_COUNT; i++) f << ' ' << p.up[i];
    f << "\nbandages " << p.bandages << "\nday " << dayT << "\nkills " << kills << "\n";
    hasSave = true;
}

bool Game::load() {
    std::ifstream f(savePath());
    if (!f) return false;
    newGame();
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string k;
        ss >> k;
        if (k == "coins") ss >> p.coins;
        else if (k == "mats") for (int i = 0; i < M_COUNT; i++) ss >> p.mats[i];
        else if (k == "wpn") for (int i = 0; i < W_COUNT; i++) ss >> p.wpn[i];
        else if (k == "up") for (int i = 0; i < U_COUNT; i++) ss >> p.up[i];
        else if (k == "bandages") ss >> p.bandages;
        else if (k == "day") ss >> dayT;
        else if (k == "kills") ss >> kills;
    }
    for (int i = 0; i < U_COUNT; i++) p.up[i] = std::min(2, std::max(0, p.up[i]));
    p.maxhp = (float)ARMOR_HP[p.up[U_ARMOR]];
    p.hp = p.maxhp;
    return true;
}

// ================================================================= helpers
void Game::newGame() {
    p = Player();
    p.pos = world.spawn;
    p.wpn[W_FLINT] = 8;
    p.bandages = 1;
    animals.clear();
    items.clear();
    projs.clear();
    parts.clear();
    ftexts.clear();
    decals.clear();
    msgs.clear();
    dayT = 30;
    kills = 0;
    sold = 0;
    helpT = 16;
    respawnT = 5;
    shakeAmt = 0;
    cam = p.pos - V2(LW / 2.f, LH / 2.f);
    populate();
    setCam(0, true);
}

float Game::tod() const { return fmodf(dayT / 300.f + 0.05f, 1.f); }
float Game::daylight() const { return smoothf(-0.25f, 0.35f, sinf(tod() * 6.2832f)); }

void Game::msg(const std::string& s, u32 col) {
    msgs.push_back({s, col, 4.2f});
    if (msgs.size() > 4) msgs.pop_front();
}
void Game::ftext(V2 pp, const std::string& s, u32 col, float life) { ftexts.push_back({pp, s, col, 0, life}); }
void Game::bloodBurst(V2 pp, V2 dir, int n) {
    for (int i = 0; i < n; i++) {
        float a = atan2f(dir.y, dir.x) + rng.range(-0.9f, 0.9f), sp = rng.range(30, 110);
        parts.push_back({pp.x, pp.y, rng.range(2, 8), cosf(a) * sp, sinf(a) * sp * 0.6f, rng.range(40, 110), 0.9f, 0.9f,
                         rng.chance(0.5f) ? HEX(0xb52a2e) : HEX(0x8c1d22), rng.chance(0.3f) ? 2 : 1, true});
    }
}
void Game::puff(V2 pp, int n, u32 col) {
    for (int i = 0; i < n; i++)
        parts.push_back({pp.x + rng.range(-3, 3), pp.y, rng.range(0, 3), rng.range(-14, 14), rng.range(-6, 6), rng.range(10, 36), 0.5f, 0.5f, col, 1, false});
}
bool Game::nearShop() const { return dist(p.pos, world.keeper) < 44 || dist(p.pos, world.door) < 36; }

Animal* Game::nearestCarcass(float maxD) {
    Animal* best = nullptr;
    float bd = maxD;
    for (auto& a : animals) {
        if (!a.dead || a.totalRemain <= 0 || a.gone) continue;
        float d = dist(p.pos, a.pos);
        if (d < bd) {
            bd = d;
            best = &a;
        }
    }
    return best;
}

// ================================================================= animals
bool Game::trySpawn(int kind, float minDist, bool offscreen) {
    static const float zr[A_COUNT][2] = {{140, 520}, {240, 700}, {360, 820}, {460, 900}};
    static const float bh[A_COUNT] = {4, 12, 7, 9};
    for (int it = 0; it < 80; it++) {
        float ang = rng.range(0, 6.2832f), rad = rng.range(zr[kind][0], zr[kind][1]);
        V2 pos = world.village + V2(cosf(ang) * rad, sinf(ang) * rad * 0.8f);
        if (pos.x < 56 || pos.y < 56 || pos.x > world.pixW() - 56 || pos.y > world.pixH() - 56) continue;
        if (world.blocked(pos.x, pos.y, 9)) continue;
        if (dist(pos, p.pos) < minDist) continue;
        if (offscreen) {
            float sx = pos.x - cam.x, sy = pos.y - cam.y;
            if (sx > -50 && sx < LW + 50 && sy > -60 && sy < LH + 60) continue;
        }
        Animal a;
        a.kind = kind;
        a.variant = (kind == A_DEER && rng.chance(0.45f)) ? 1 : 0;
        a.pos = pos;
        a.target = pos;
        a.maxhp = a.hp = ANIMAL[kind].hp;
        a.face = rng.chance(0.5f) ? 1 : -1;
        a.bias = rng.chance(0.5f) ? 1.f : -1.f;
        a.idleT = rng.range(0, 3);
        a.animT = rng.range(0, 4);
        a.bodyH = bh[kind];
        animals.push_back(a);
        return true;
    }
    return false;
}

void Game::populate() {
    animals.clear();
    int counts[A_COUNT] = {14, 8, 5, 4};
    for (int k = 0; k < A_COUNT; k++)
        for (int i = 0; i < counts[k]; i++) trySpawn(k, 110, false);
}

bool Game::stepAnimal(Animal& a, V2 dir, float speed, float dt) {
    static const float offs[7] = {0, 0.6f, -0.6f, 1.2f, -1.2f, 1.9f, -1.9f};
    float base = atan2f(dir.y, dir.x);
    float r = a.kind == A_RABBIT ? 3.5f : 5.5f;
    // rabbits move in hops
    float sp = speed;
    if (a.kind == A_RABBIT) sp *= 0.35f + 0.9f * std::max(0.f, sinf(a.animT * 1.5708f * 2));
    for (int k = 0; k < 7; k++) {
        float ang = base + offs[k] * a.bias;
        V2 d(cosf(ang), sinf(ang));
        V2 np = a.pos + d * (sp * dt);
        if (!world.blocked(np.x, np.y, r)) {
            if (k > 0 && (k % 2 == 0)) a.bias = -a.bias;
            a.pos = np;
            a.vel = d * sp;
            return true;
        }
    }
    a.vel = V2();
    return false;
}

void Game::updateAnimals(float dt) {
    float night = 1.f - daylight();
    for (auto& a : animals) {
        if (a.flash > 0) a.flash -= dt;
        if (a.hpShow > 0) a.hpShow -= dt;
        if (a.atkCd > 0) a.atkCd -= dt;
        if (a.kb.len() > 3) {
            moveEnt(world, a.pos, a.kb * dt, 5.f);
            a.kb = a.kb * expf(-9 * dt);
        } else
            a.kb = V2();
        if (a.dead) {
            a.deadT += dt;
            if (a.deadT > 170) a.fade -= dt / 8.f;
            if (a.totalRemain <= 0) a.fade -= dt * 1.6f;
            if (a.fade <= 0) a.gone = true;
            continue;
        }
        const AnimalDef& D = ANIMAL[a.kind];
        V2 dp = p.pos - a.pos;
        float dd = p.dead ? 9999.f : dp.len();
        float notice = D.notice * (D.aggro ? 1.f + 0.3f * night : 1.f);
        a.stT -= dt;
        a.vel = V2();
        switch (a.st) {
            case AS_WANDER: {
                bool alert = false;
                if (!D.aggro) alert = dd < notice;
                else if (a.kind == A_WOLF) alert = dd < notice;
                else alert = dd < notice * 0.62f;
                if (alert) {
                    if (D.aggro) {
                        a.st = AS_CHASE;
                        a.stT = 12;
                        ftext(a.pos + V2(0, -a.bodyH - 12), "!", C_RED, 0.8f);
                    } else {
                        a.st = AS_FLEE;
                        a.stT = rng.range(2.5f, 4.f);
                        ftext(a.pos + V2(0, -a.bodyH - 12), "!", C_TEXT, 0.7f);
                    }
                    break;
                }
                if (a.idleT > 0) {
                    a.idleT -= dt;
                } else {
                    V2 to = a.target - a.pos;
                    if (to.len() < 5 || a.stT < -7) {
                        a.idleT = rng.range(1.2f, 4.5f);
                        a.target = a.pos + V2(rng.range(-90, 90), rng.range(-70, 70));
                        a.stT = 0;
                    } else if (!stepAnimal(a, to.norm(), D.wander, dt)) {
                        a.target = a.pos + V2(rng.range(-60, 60), rng.range(-50, 50));
                    }
                }
                break;
            }
            case AS_FLEE: {
                V2 away = (a.pos - p.pos).norm();
                if (away.len() < 0.1f) away = V2(1, 0);
                V2 perp(-away.y, away.x);
                float j = sinf(timeAcc * 2.3f + a.animT) * 0.35f;
                V2 dir = (away + perp * j).norm();
                stepAnimal(a, dir, D.run * (a.hp < a.maxhp ? 1.08f : 1.f), dt);
                if (a.stT <= 0 && dd > notice * 1.4f) {
                    a.st = AS_WANDER;
                    a.idleT = 1;
                    a.target = a.pos;
                }
                break;
            }
            case AS_CHASE: {
                V2 toP = dp.norm();
                if (a.kind == A_WOLF && a.hp < a.maxhp * 0.25f) {
                    a.st = AS_FLEE;
                    a.stT = 6;
                    break;
                }
                if (dd > (a.kind == A_WOLF ? 300.f : 240.f) || p.dead) {
                    a.st = AS_WANDER;
                    a.idleT = 1.5f;
                    a.target = a.pos;
                    break;
                }
                a.face = dp.x > 0 ? 1 : -1;
                if (dd < D.hitR + 9) {
                    if (a.atkCd <= 0) {
                        hurtPlayer(D.dmg, a.pos);
                        a.atkCd = D.atkCd;
                        a.kb = toP * -70;
                    }
                } else
                    stepAnimal(a, toP, D.run * (dd < 50 ? 1.12f : 1.f), dt);
                break;
            }
        }
        if (fabsf(a.vel.x) > 2.f && a.st != AS_CHASE) a.face = a.vel.x > 0 ? 1 : -1;
        else if (fabsf(a.vel.x) > 2.f) a.face = a.vel.x > 0 ? 1 : -1;
        float spd = a.vel.len();
        if (spd > 1) a.animT += spd * dt * (a.kind == A_RABBIT ? 0.09f : 0.075f);
        a.noticeT += dt;
    }
    animals.erase(std::remove_if(animals.begin(), animals.end(), [](const Animal& a) { return a.gone; }), animals.end());
}

void Game::hitAnimal(Animal& a, float dmg, V2 dir, bool crit) {
    int d = std::max(1, (int)roundf(dmg));
    a.hp -= d;
    a.flash = 0.16f;
    a.hpShow = 3.0f;
    a.kb += dir * (crit ? 150.f : 105.f);
    bloodBurst(a.pos + V2(0, -a.bodyH), dir, crit ? 14 : 8);
    ftext(a.pos + V2(0, -a.bodyH - 10), (crit ? std::string("CRIT ") : std::string()) + S(d), crit ? C_ORANGE : C_TEXT, 0.9f);
    audio.play(crit ? S_CRIT : S_HIT);
    shakeAmt = std::max(shakeAmt, crit ? 3.f : 1.5f);
    decals.push_back({a.pos + V2(rng.range(-5, 5), rng.range(0, 4)), 0, 0, 40});
    const AnimalDef& D = ANIMAL[a.kind];
    if (D.aggro) {
        a.st = AS_CHASE;
        a.stT = 10;
    } else {
        a.st = AS_FLEE;
        a.stT = 7;
    }
    if (a.hp <= 0) killAnimal(a);
}

void Game::killAnimal(Animal& a) {
    a.dead = true;
    a.deadT = 0;
    a.hp = 0;
    a.vel = V2();
    kills++;
    const AnimalDef& D = ANIMAL[a.kind];
    int total = 0;
    for (int m = 0; m < M_COUNT; m++) {
        a.remain[m] = D.drop[m][1] > 0 ? rng.irange(D.drop[m][0], D.drop[m][1]) : 0;
        total += a.remain[m];
    }
    if (a.kind == A_DEER && a.variant == 1) {
        int n = rng.irange(1, 2);
        a.remain[M_ANTLER] = n;
        total += n;
    }
    if (total == 0) {
        a.remain[M_MEAT] = 1;
        total = 1;
    }
    a.totalRemain = total;
    static const int sz[A_COUNT] = {0, 2, 1, 1};
    decals.push_back({a.pos + V2(0, 3), sz[a.kind], 0, 80});
    bloodBurst(a.pos + V2(0, -a.bodyH), a.kb.norm(), 16);
    audio.play(S_KILL);
    shakeAmt = std::max(shakeAmt, 4.f);
    ftext(a.pos + V2(0, -a.bodyH - 18), "HUNTED!", C_GOLD, 1.4f);
}

// ================================================================= player
void Game::hurtPlayer(float dmg, V2 from) {
    if (p.invuln > 0 || p.dead) return;
    int d = std::max(1, (int)ceilf(dmg * (1.f - ARMOR_RED[p.up[U_ARMOR]])));
    p.hp -= d;
    p.invuln = 0.85f;
    p.sinceHurt = 0;
    V2 dir = (p.pos - from).norm();
    p.kb = dir * 140;
    shakeAmt = 5.5f;
    audio.play(S_HURT);
    ftext(p.pos + V2(0, -24), "-" + S(d), C_RED, 1.0f);
    bloodBurst(p.pos + V2(0, -8), dir, 9);
    if (p.hp <= 0) {
        p.hp = 0;
        p.dead = true;
        p.deadT = 0;
        p.charging = false;
        int loss = (int)(p.coins * 0.15f);
        p.coins -= loss;
        msg("YOU WERE KNOCKED OUT!  LOST " + S(loss) + " COINS", C_RED);
    }
}

void Game::throwWeapon() {
    int w = p.sel;
    if (p.wpn[w] <= 0) return;
    p.wpn[w]--;
    const WpnDef& D = WPN[w];
    V2 hand = p.pos + V2(0, -10);
    V2 mw(in.mx + camX, in.my + camY);
    V2 d = mw - hand;
    float dl = d.len();
    V2 dir = dl > 4 ? d * (1.f / dl) : p.aim;
    float c = p.charge;
    Proj pr;
    pr.pos = hand + dir * 8;
    pr.last = pr.pos;
    pr.dir = dir;
    pr.speed = D.speed * (1 + 0.15f * c);
    pr.range = std::min(D.range * (1 + 0.15f * c), dl + 24);
    pr.travelled = 0;
    pr.wpn = w;
    pr.angle = atan2f(dir.y, dir.x);
    pr.spin = (w == W_JAVELIN) ? 0.f : (w == W_AXE ? 17.f : 26.f);
    pr.crit = rng.chance(0.10f);
    pr.dmg = D.dmg * (1 + 0.4f * c) * rng.range(0.9f, 1.1f) * (pr.crit ? 1.6f : 1.f);
    projs.push_back(pr);
    p.cd = D.cd;
    p.throwAnim = 0.3f;
    audio.play(S_THROW, 0.6f);
    if (p.wpn[w] <= 0) {
        for (int i = 0; i < W_COUNT; i++)
            if (p.wpn[i] > 0) {
                p.sel = i;
                break;
            }
    }
}

void Game::addItem(int m, int n) { p.mats[m] += n; }

void Game::gatherStep(Animal& c) {
    int m = -1;
    for (int i = 0; i < M_COUNT; i++) {
        int mm = GATHER_ORDER[i];
        if (c.remain[mm] > 0) {
            m = mm;
            break;
        }
    }
    if (m < 0) {
        c.totalRemain = 0;
        return;
    }
    if (p.bagUsed() >= p.bagCap()) {
        if (bagWarn <= 0) {
            ftext(p.pos + V2(0, -28), "BAG FULL!", C_RED, 1.2f);
            msg("BAG FULL - SELL AT THE SHOP", C_RED);
            audio.play(S_ERROR);
            bagWarn = 1.6f;
        }
        return;
    }
    c.remain[m]--;
    c.totalRemain--;
    p.mats[m]++;
    int extra = 0;
    if (rng.chance(TOOL_BONUS[p.up[U_TOOL]]) && p.bagUsed() < p.bagCap()) {
        p.mats[m]++;
        extra = 1;
    }
    ftext(c.pos + V2(rng.range(-6, 6), -10), "+" + S(1 + extra) + " " + MAT_NAME[m], extra ? C_GOLD : C_GREEN, 1.2f);
    bloodBurst(c.pos + V2(0, -4), V2(rng.range(-1, 1), -0.6f), 4);
    puff(c.pos + V2(0, -2), 3, HEX(0xd8c8a8));
    audio.play(S_GATHER);
    if (c.totalRemain <= 0) {
        ftext(c.pos + V2(0, -22), "CLEANED", C_TEXT, 1.0f);
        audio.play(S_PICKUP, 0.4f);
    }
}

void Game::updatePlayer(float dt) {
    Player& P = p;
    float newMax = (float)ARMOR_HP[P.up[U_ARMOR]];
    if (newMax > P.maxhp) P.hp += newMax - P.maxhp;
    P.maxhp = newMax;
    if (P.dead) {
        P.deadT += dt;
        if (P.deadT > 3.4f) {
            P.pos = world.spawn;
            P.hp = P.maxhp;
            P.dead = false;
            P.invuln = 2.f;
            P.kb = V2();
            for (auto& a : animals)
                if (!a.dead && a.st == AS_CHASE) {
                    a.st = AS_WANDER;
                    a.idleT = 2;
                }
            setCam(0, true);
        }
        return;
    }
    P.cd = std::max(0.f, P.cd - dt);
    P.invuln = std::max(0.f, P.invuln - dt);
    P.sinceHurt += dt;
    P.throwAnim = std::max(0.f, P.throwAnim - dt);

    if (in.selectKey >= 0 && in.selectKey < W_COUNT) P.sel = in.selectKey;
    if (in.wheel != 0) P.sel = ((P.sel + (in.wheel > 0 ? -1 : 1)) % W_COUNT + W_COUNT) % W_COUNT;

    V2 mw(in.mx + camX, in.my + camY);
    V2 hand = P.pos + V2(0, -10);
    V2 aimv = mw - hand;
    if (aimv.len() > 1) P.aim = aimv.norm();

    Animal* c = nearestCarcass(26);
    bool wantGather = in.e && c != nullptr && !P.charging;

    V2 mv((in.right ? 1.f : 0.f) - (in.left ? 1.f : 0.f), (in.down ? 1.f : 0.f) - (in.up ? 1.f : 0.f));
    bool moving = mv.len() > 0.1f;
    if (moving) mv = mv.norm();
    float spd = 70.f * BOOT_MUL[P.up[U_BOOTS]];
    bool sprinting = false;
    if (in.sprint && moving && !P.exhausted && P.stamina > 0 && !wantGather) {
        sprinting = true;
        spd *= 1.55f;
        P.stamina -= 24 * dt;
        P.staDelay = 0.7f;
        if (P.stamina <= 0) {
            P.stamina = 0;
            P.exhausted = true;
        }
    } else {
        P.staDelay -= dt;
        if (P.staDelay <= 0) P.stamina = std::min(100.f, P.stamina + (moving ? 16.f : 32.f) * dt);
        if (P.exhausted && P.stamina > 30) P.exhausted = false;
    }
    if (P.charging) spd *= 0.6f;
    if (wantGather) spd = 0;
    V2 vel = mv * spd + P.kb;
    P.kb = P.kb * expf(-9 * dt);
    moveEnt(world, P.pos, vel * dt, 4.f);
    P.pos.x = clampf(P.pos.x, 20, world.pixW() - 20);
    P.pos.y = clampf(P.pos.y, 20, world.pixH() - 20);

    auto setDir = [&](V2 d) {
        if (fabsf(d.x) > fabsf(d.y) * 0.9f) {
            P.dir = 2;
            P.flip = d.x < 0;
        } else
            P.dir = d.y > 0 ? 0 : 1;
    };
    if (wantGather) setDir(c->pos - P.pos);
    else if (P.charging || P.throwAnim > 0) setDir(P.aim);
    else if (moving) setDir(mv);
    if (moving && !wantGather) P.animT += dt * (sprinting ? 12.f : 8.5f);
    if (sprinting && rng.chance(0.25f)) puff(P.pos + V2(0, -1), 1, HEX(0xcdb98f));

    // --- throwing
    if (in.lPressed && !wantGather && P.wpn[P.sel] > 0 && P.cd <= 0) {
        P.charging = true;
        P.charge = 0;
    }
    if (P.charging) {
        if (P.wpn[P.sel] <= 0) P.charging = false;
        else {
            P.charge = std::min(1.f, P.charge + dt / 0.5f);
            if (in.lReleased || !in.l) {
                throwWeapon();
                P.charging = false;
                P.charge = 0;
            }
        }
    }

    // --- pick thrown weapons back up
    for (size_t i = 0; i < items.size();) {
        GroundItem& it = items[i];
        if (dist(P.pos, it.pos) < 13 && P.wpn[it.wpn] < WPN_MAX) {
            P.wpn[it.wpn]++;
            ftext(P.pos + V2(0, -24), std::string("+") + WPN[it.wpn].name, C_BLUE, 1.0f);
            audio.play(S_PICKUP, 0.5f);
            items.erase(items.begin() + i);
        } else
            i++;
    }

    // --- gathering
    if (wantGather) {
        P.gathering = true;
        float need = ANIMAL[c->kind].gatherTime;
        P.gatherT += dt * TOOL_SPEED[P.up[U_TOOL]];
        P.gatherSfx -= dt;
        if (P.gatherSfx <= 0) {
            P.gatherSfx = 0.17f / TOOL_SPEED[P.up[U_TOOL]];
            puff(c->pos + V2(rng.range(-5, 5), -3), 1, HEX(0xd8c8a8));
        }
        if (P.gatherT >= need) {
            P.gatherT = 0;
            gatherStep(*c);
        }
    } else {
        P.gathering = false;
        P.gatherT = std::max(0.f, P.gatherT - dt * 2.f);
    }

    // --- shop / bandage / regen
    if (in.ePressed && !c && nearShop()) {
        state = ST_SHOP;
        audio.play(S_CLICK);
    }
    if (in.bandage && P.bandages > 0 && P.hp < P.maxhp) {
        P.bandages--;
        P.hp = std::min(P.maxhp, P.hp + BANDAGE_HEAL);
        ftext(P.pos + V2(0, -24), "+" + S(BANDAGE_HEAL) + " HP", C_GREEN, 1.1f);
        audio.play(S_PICKUP);
    }
    if (P.sinceHurt > 6.f) P.hp = std::min(P.maxhp, P.hp + (nearShop() ? 12.f : 1.6f) * dt);
}

void Game::updateProjectiles(float dt) {
    for (size_t i = 0; i < projs.size();) {
        Proj& pr = projs[i];
        bool done = false;
        float step = pr.speed * dt;
        int sub = std::max(1, (int)ceilf(step / 3.f));
        float s = step / sub;
        for (int k = 0; k < sub && !done; k++) {
            pr.last = pr.pos;
            pr.pos += pr.dir * s;
            pr.travelled += s;
            for (auto& a : animals) {
                if (a.dead) continue;
                V2 c = a.pos + V2(0, -a.bodyH);
                if (dist(pr.pos, c) < ANIMAL[a.kind].hitR + 3) {
                    hitAnimal(a, pr.dmg, pr.dir, pr.crit);
                    if (rng.chance(WPN[pr.wpn].brk)) {
                        for (int q = 0; q < 6; q++)
                            parts.push_back({pr.pos.x, pr.pos.y, 4, rng.range(-50, 50), rng.range(-30, 30), rng.range(30, 80), 0.5f, 0.5f, HEX(0xd8dce4), 1, true});
                        audio.play(S_BREAK, 0.6f);
                        ftext(a.pos + V2(0, -a.bodyH - 22), "SNAP", C_DIM, 0.7f);
                    } else {
                        items.push_back({a.pos + V2(rng.range(-9, 9), rng.range(0, 7)), pr.wpn, rng.range(0, 6.28f), 0});
                    }
                    done = true;
                    break;
                }
            }
            if (done) break;
            V2 g = pr.pos + V2(0, 9);
            int tx = (int)floorf(g.x / TS), ty = (int)floorf(g.y / TS);
            bool obstacle = tx < 0 || ty < 0 || tx >= MW || ty >= MH || world.solid[ty][tx];
            if (obstacle) {
                V2 lg = pr.last + V2(0, 9);
                items.push_back({lg, pr.wpn, pr.angle + pr.spin * (pr.travelled / pr.speed), 0});
                for (int q = 0; q < 4; q++)
                    parts.push_back({pr.pos.x, pr.pos.y, 3, rng.range(-40, 40), rng.range(-20, 20), rng.range(20, 60), 0.4f, 0.4f, HEX(0xffe9a8), 1, true});
                audio.play(S_CLICK, 0.5f);
                done = true;
            } else if (pr.travelled >= pr.range) {
                if (world.water(tx, ty)) {
                    for (int q = 0; q < 8; q++)
                        parts.push_back({g.x, g.y, 1, rng.range(-30, 30), rng.range(-15, 15), rng.range(30, 70), 0.6f, 0.6f, HEX(0xcfe9f5), 1, true});
                    ftext(g + V2(0, -8), "SPLASH", C_BLUE, 0.8f);
                } else {
                    float ang = pr.angle + pr.spin * (pr.travelled / pr.speed);
                    items.push_back({g, pr.wpn, ang, 0});
                    puff(g, 3, HEX(0xcdb98f));
                }
                done = true;
            }
        }
        if (done) projs.erase(projs.begin() + i);
        else i++;
    }
}

void Game::updateFx(float dt) {
    for (size_t i = 0; i < parts.size();) {
        Part& q = parts[i];
        q.life -= dt;
        q.x += q.vx * dt;
        q.y += q.vy * dt;
        q.z += q.vz * dt;
        if (q.grav) q.vz -= 260 * dt;
        if (q.z < 0) {
            q.z = 0;
            q.vx *= 0.4f;
            q.vy *= 0.4f;
            q.vz = 0;
            q.life -= dt * 2;
        }
        if (q.life <= 0) parts.erase(parts.begin() + i);
        else i++;
    }
    for (size_t i = 0; i < ftexts.size();) {
        ftexts[i].t += dt;
        ftexts[i].p.y -= 14 * dt;
        if (ftexts[i].t >= ftexts[i].life) ftexts.erase(ftexts.begin() + i);
        else i++;
    }
    for (size_t i = 0; i < decals.size();) {
        decals[i].t += dt;
        if (decals[i].t >= decals[i].life) decals.erase(decals.begin() + i);
        else i++;
    }
    for (size_t i = 0; i < items.size();) {
        items[i].t += dt;
        if (items[i].t > 300) items.erase(items.begin() + i);
        else i++;
    }
    for (auto& m : msgs) m.t -= dt;
    while (!msgs.empty() && msgs.front().t <= 0) msgs.pop_front();
    for (auto& f : flies) {
        f.x += (sinf(timeAcc * 0.7f * f.sp + f.ph) * 9 + 4) * dt;
        f.y += cosf(timeAcc * 0.9f * f.sp + f.ph * 2) * 7 * dt;
        if (f.x > LW + 4) f.x = -4;
        if (f.x < -4) f.x = LW + 4;
        if (f.y > LH + 4) f.y = -4;
        if (f.y < -4) f.y = LH + 4;
    }
    shakeAmt = std::max(0.f, shakeAmt - 16.f * dt);
}

void Game::setCam(float dt, bool snap) {
    V2 target = p.pos + V2(0, -8) - V2(LW / 2.f, LH / 2.f);
    if (snap) cam = target;
    else {
        float k = std::min(1.f, dt * 7.f);
        cam.x += (target.x - cam.x) * k;
        cam.y += (target.y - cam.y) * k;
    }
    cam.x = clampf(cam.x, 0, world.pixW() - LW);
    cam.y = clampf(cam.y, 0, world.pixH() - LH);
    float sx = 0, sy = 0;
    if (shakeAmt > 0.2f) {
        sx = rng.range(-1, 1) * shakeAmt * 0.6f;
        sy = rng.range(-1, 1) * shakeAmt * 0.6f;
    }
    camX = (int)roundf(cam.x + sx);
    camY = (int)roundf(cam.y + sy);
}

// Safety net against a soft-lock: if the hunter has no weapons, none lying on the ground or in
// flight, and can't afford a new flint pack even by selling everything, the merchant lends a few
// knives. Checked about once a second; it never triggers for a player who can still recover.
void Game::checkWeaponSafetyNet(float dt) {
    stuckCheckT -= dt;
    if (stuckCheckT > 0) return;
    stuckCheckT = 1.f;
    if (p.dead || p.charging) return;
    for (int count : p.wpn)
        if (count > 0) return;                // still holding a weapon
    if (!projs.empty() || !items.empty()) return;  // a thrown weapon can still be picked up
    int wealth = p.coins;
    for (int m = 0; m < M_COUNT; m++) wealth += p.mats[m] * MAT_PRICE[m];
    if (wealth >= FLINT_PACK_PRICE) return;  // could buy flint at the shop
    p.wpn[W_FLINT] = FLINT_LOAN;
    p.sel = W_FLINT;
    msg("THE MERCHANT LENDS YOU " + S(FLINT_LOAN) + " FLINT KNIVES", C_GOLD);
    ftext(p.pos + V2(0, -28), "+" + S(FLINT_LOAN) + " FLINT KNIFE", C_BLUE, 1.4f);
    audio.play(S_PICKUP);
}

void Game::updatePlay(float dt) {
    dayT += dt;
    if (helpT > 0) helpT -= dt;
    if (bagWarn > 0) bagWarn -= dt;
    updatePlayer(dt);
    updateAnimals(dt);
    updateProjectiles(dt);
    checkWeaponSafetyNet(dt);
    updateFx(dt);
    setCam(dt);
    // keep the wilds populated
    respawnT -= dt;
    if (respawnT <= 0) {
        respawnT = 4.f;
        int alive[A_COUNT] = {0};
        for (auto& a : animals)
            if (!a.dead) alive[a.kind]++;
        int targets[A_COUNT] = {14, 8, 5, 4 + (int)(2 * (1.f - daylight()))};
        for (int k = 0; k < A_COUNT; k++)
            if (alive[k] < targets[k]) trySpawn(k, 230, true);
    }
}

void Game::update(float dt) {
    timeAcc += dt;
    switch (state) {
        case ST_TITLE: {
            titleT += dt;
            dayT += dt * 0.6f;
            updateAnimals(dt);
            updateFx(dt);
            V2 c = world.village + V2(cosf(titleT * 0.12f) * 110, sinf(titleT * 0.12f) * 55 + 10);
            cam = c - V2(LW / 2.f, LH / 2.f);
            cam.x = clampf(cam.x, 0, world.pixW() - LW);
            cam.y = clampf(cam.y, 0, world.pixH() - LH);
            camX = (int)roundf(cam.x);
            camY = (int)roundf(cam.y);
            if (in.enterPressed) {
                newGame();
                state = ST_PLAY;
                audio.play(S_CLICK);
            }
            break;
        }
        case ST_PLAY:
            if (in.escPressed) {
                state = ST_PAUSE;
                break;
            }
            if (in.mapToggle) showMap = !showMap;
            if (in.helpToggle) helpT = helpT > 0 ? 0 : 14;
            updatePlay(dt);
            break;
        case ST_SHOP:
            dayT += 0;  // time stands still while trading
            updateFx(dt);
            if (in.escPressed || in.ePressed) {
                state = ST_PLAY;
                save();
                audio.play(S_CLICK);
            }
            if (in.tabPressed) shopTab = (shopTab + 1) % 4;
            if (in.selectKey >= 0 && in.selectKey < 4) shopTab = in.selectKey;
            break;
        case ST_PAUSE:
            if (in.escPressed) state = ST_PLAY;
            break;
    }
}

// ================================================================= economy
bool Game::canAfford(int coins, int m1, int n1, int m2, int n2) const {
    if (p.coins < coins) return false;
    if (m1 >= 0 && p.mats[m1] < n1) return false;
    if (m2 >= 0 && p.mats[m2] < n2) return false;
    return true;
}
void Game::spend(int coins, int m1, int n1, int m2, int n2) {
    p.coins -= coins;
    if (m1 >= 0) p.mats[m1] -= n1;
    if (m2 >= 0) p.mats[m2] -= n2;
}
bool Game::buyUpgrade(int u) {
    int lv = p.up[u];
    if (lv >= 2) return false;
    const UpLevel& L = UPS[u].lv[lv];
    if (!canAfford(L.coins, L.m1, L.n1, L.m2, L.n2)) {
        audio.play(S_ERROR);
        return false;
    }
    spend(L.coins, L.m1, L.n1, L.m2, L.n2);
    p.up[u]++;
    audio.play(S_CRAFT);
    msg(std::string("UPGRADED: ") + L.name, C_GOLD);
    save();
    return true;
}
bool Game::craft(int r) {
    const Recipe& R = RECIPES[r];
    for (int i = 0; i < 3; i++)
        if (R.mat[i] >= 0 && p.mats[R.mat[i]] < R.n[i]) {
            audio.play(S_ERROR);
            return false;
        }
    if (p.wpn[R.out] >= WPN_MAX) {
        msg("YOU CAN'T CARRY ANY MORE " + std::string(WPN[R.out].name), C_RED);
        audio.play(S_ERROR);
        return false;
    }
    for (int i = 0; i < 3; i++)
        if (R.mat[i] >= 0) p.mats[R.mat[i]] -= R.n[i];
    p.wpn[R.out] = std::min(WPN_MAX, p.wpn[R.out] + R.count);
    audio.play(S_CRAFT);
    msg("CRAFTED " + S(R.count) + " " + WPN[R.out].name, C_GOLD);
    return true;
}
void Game::sellMat(int m, int n) {
    n = std::min(n, p.mats[m]);
    if (n <= 0) return;
    p.mats[m] -= n;
    p.coins += n * MAT_PRICE[m];
    sold += n * MAT_PRICE[m];
    audio.play(S_COIN);
}
