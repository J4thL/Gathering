// selftest_weather.cpp - `--selftest`: the weather schedule, the weather state machine and what rain / sandstorms do to the hunter.
#include "tests/selftest_ctx.hpp"

void testWeather(SelfTest& t) {
    Game& g = t.g;
    auto check = [&](bool ok, const char* what) { t.check(ok, what); };
    printf("weather\n");
    // keep what we touch
    const uint32_t seed0 = g.worldSeed;
    const float day0 = g.dayT, storm0 = g.stormK, gale0 = g.galeK, wind0 = g.wind, wet0 = g.wet, windT0 = g.windT, drift0 = g.rainDrift, gust0 = g.windGust;
    const int interior0 = g.interior, delve0 = g.delve, wx0 = g.wxKind;
    const auto state0 = g.state;
    float bw0[B_COUNT];
    for (int b = 0; b < B_COUNT; b++) bw0[b] = g.bw[b];
    auto setBiome = [&](int biome) {
        for (int b = 0; b < B_COUNT; b++) g.bw[b] = b == biome ? 1.f : 0.f;
    };
    // first time at or after 100 s that `kind` has been blowing for at least minAge seconds (-1 if never)
    auto findActive = [&](int kind, float minAge) {
        for (double tt = 100; tt < 100000; tt += 1) {
            float age = 0;
            if (g.weatherAt(kind, tt, &age) && age >= minAge) return tt;
        }
        return -1.0;
    };

    // ---- the schedule: a pure function of the world seed and the clock
    g.worldSeed = 12345;
    bool same = true, quietStart = true, durOk = true;
    int stormSecs = 0, sandSecs = 0, snowSecs = 0, samples = 0;
    for (double tt = 0; tt < 36000; tt += 1) {
        float age = 0, left = 0;
        const bool a = g.weatherAt(WX_STORM, tt, &age, &left), b = g.weatherAt(WX_STORM, tt);
        same = same && a == b;
        if (tt < 100) quietStart = quietStart && !a && !g.weatherAt(WX_SANDSTORM, tt) && !g.weatherAt(WX_BLIZZARD, tt);
        if (a) {
            stormSecs++;
            const float dur = age + left;
            durOk = durOk && dur >= WEATHER[WX_STORM].durLo - 0.01f && dur <= WEATHER[WX_STORM].durHi + 0.01f;
        }
        sandSecs += g.weatherAt(WX_SANDSTORM, tt);
        snowSecs += g.weatherAt(WX_BLIZZARD, tt);
        samples++;
    }
    check(same, "the schedule is a pure function: asking twice gives the same answer");
    check(quietStart, "a new game starts with 100 s of calm in every climate");
    check(durOk, "every storm lasts between its shortest and its longest duration");
    check(stormSecs > samples * 0.08 && stormSecs < samples * 0.25, "storms fill about a seventh of the time");
    check(sandSecs > 0 && snowSecs > 0, "sandstorms and blizzards happen too");
    check(!g.weatherAt(WX_CLEAR, 500) && !g.weatherAt(WX_COUNT, 500) && !g.weatherAt(WX_STORM, -5), "clear weather and nonsense inputs are never 'blowing'");
    bool differs = false;
    for (double tt = 100; tt < 36000 && !differs; tt += 1) {
        g.worldSeed = 12345;
        const bool a = g.weatherAt(WX_STORM, tt);
        g.worldSeed = 999;
        differs = a != g.weatherAt(WX_STORM, tt);
    }
    check(differs, "another world seed has different weather");

    // ---- the state machine follows the schedule
    g.worldSeed = 12345;
    g.state = ST_PLAY;
    g.interior = -1;
    g.delve = -1;
    setBiome(B_HOME);
    g.stormK = g.galeK = 0.f;
    const double ts = findActive(WX_STORM, 30.f);  // past the 15 s drizzle
    check(ts > 0, "test setup: a storm exists in the schedule");
    if (ts > 0) {
        g.dayT = (float)ts;
        for (int i = 0; i < 120; i++) g.updateLight(0.1f);  // 12 s of game time
        check(g.wxKind == WX_STORM && g.stormK > 0.9f, "in the forest at a scheduled storm, the storm breaks");
        check(g.rainK() > 0.9f && g.galeNow() == 0.f, "it is rain, not a sandstorm");
        g.structs.clear();
        float drain = 1, regen = 1;
        g.climateMods(drain, regen);
        check(drain > 1.1f && regen < 0.8f, "soaked: stamina drains faster and returns slower in the rain");
        g.interior = 0;  // walk into a house
        check(g.rainK() == 0.f, "no rain indoors");
        for (int i = 0; i < 80; i++) g.updateLight(0.1f);
        check(g.wxKind == WX_CLEAR && g.stormK < 0.05f, "indoors the weather is gone and the storm fades");
        g.interior = -1;
        setBiome(B_DESERT);
        g.stormK = 0.f;
        for (int i = 0; i < 20; i++) g.updateLight(0.1f);
        check(g.stormK == 0.f && g.wxKind != WX_STORM, "no rain in the desert");
    }
    setBiome(B_DESERT);
    const double ds = findActive(WX_SANDSTORM, 5.f);
    check(ds > 0, "test setup: a sandstorm exists in the schedule");
    if (ds > 0) {
        g.dayT = (float)ds;
        g.stormK = g.galeK = 0.f;
        for (int i = 0; i < 100; i++) g.updateLight(0.1f);
        check(g.wxKind == WX_SANDSTORM && g.galeK > 0.9f && g.stormK == 0.f, "in the desert at a scheduled sandstorm, the sand blows");
    }

    // ---- the weather particles: a pool of FLAKES_MAX, FLAKES_CALM of them in the air on a calm day, more in a gale
    {
        g.stormK = g.galeK = 0.f;
        check((int)g.flakes.size() == FLAKES_MAX, "the particle pool holds FLAKES_MAX");
        check(g.activeFlakes() == FLAKES_CALM, "a calm day has FLAKES_CALM particles in the air");
        g.galeK = 1.f;
        check(g.activeFlakes() == FLAKES_MAX, "a sandstorm / blizzard fills the air");
        g.galeK = 0.5f;
        check(g.activeFlakes() > FLAKES_CALM && g.activeFlakes() < FLAKES_MAX, "half a gale, half the extra particles");
        g.galeK = 0.f;
        auto meanMove = [&](int biome, float& mdx, float& mdy) {
            setBiome(biome);
            std::vector<Flake> before = g.flakes;
            g.updateWeather(0.1f);
            double sx = 0, sy = 0;
            int n = 0;
            for (int i = 0; i < g.activeFlakes(); i++) {
                const float dx = g.flakes[i].x - before[i].x, dy = g.flakes[i].y - before[i].y;
                if (fabsf(dx) > 20.f || fabsf(dy) > 20.f) continue;  // wrapped round the screen
                sx += dx, sy += dy, n++;
            }
            mdx = n ? (float)(sx / n) : 0.f;
            mdy = n ? (float)(sy / n) : 0.f;
            bool idle = true;  // the particles beyond the active ones stay where they are
            for (int i = g.activeFlakes(); i < (int)g.flakes.size(); i++) idle = idle && g.flakes[i].x == before[i].x && g.flakes[i].y == before[i].y;
            return idle;
        };
        float dx = 0, dy = 0;
        check(meanMove(B_SNOW, dx, dy) && dy > 0.f, "snow falls, and the unused particles do not move");
        check(meanMove(B_DESERT, dx, dy) && dx > 0.f, "sand blows towards the right edge");
        g.galeK = 1.f;
        float gdx = 0;
        meanMove(B_DESERT, gdx, dy);
        check(gdx > dx * 1.5f, "in a sandstorm it blows much harder");
        g.galeK = 0.f;
        meanMove(B_MOUNTAIN, dx, dy);
        check(dx > 0.f, "mountain dust blows towards the right edge too");
    }

    // ---- the shared wind: one value the particles, the rain, the sway and the sound all read
    {
        auto run = [&](float secs) {  // advance the weather by `secs`, returns how far the sway clock and the rain moved
            const float t0 = g.windT, d0 = g.rainDrift;
            for (int i = 0; i < (int)(secs * 10); i++) g.updateWeather(0.1f);
            return std::make_pair(g.windT - t0, g.rainDrift - d0);
        };
        setBiome(B_HOME);
        g.stormK = g.galeK = 0.f;
        const auto calm = run(1.f);
        const float calmWind = g.wind;
        check(calmWind >= 0.05f && calmWind <= 0.5f, "a calm day has a light breeze");
        g.stormK = 1.f;
        const auto storm = run(1.f);
        check(g.wind > calmWind + 0.3f, "a storm blows harder than a calm day");
        check(storm.first > calm.first && storm.second > calm.second, "in a storm the trees sway faster and the rain is carried further sideways");
        g.stormK = 0.f;
        setBiome(B_DESERT);
        g.galeK = 1.f;
        run(1.f);
        check(g.wind > 1.f, "a sandstorm is a gale");
        setBiome(B_MOUNTAIN);
        g.stormK = g.galeK = 1.f;
        run(1.f);
        check(g.wind <= 2.f + 1e-4f && g.wind >= 0.f, "the wind never exceeds its range");
        g.stormK = g.galeK = 0.f;
        // ---- wet ground
        setBiome(B_HOME);
        g.interior = -1;
        g.delve = -1;
        g.wet = 0.f;
        g.stormK = 1.f;
        run(25.f);
        check(g.wet > 0.99f, "a storm soaks the ground in about 20 s");
        g.stormK = 0.f;
        run(45.f);
        check(g.wet > 0.3f && g.wet < 0.7f, "the ground stays wet for a while after the rain stops");
        run(60.f);
        check(g.wet == 0.f, "and dries out completely");
        g.wet = 0.f;
        g.stormK = 1.f;
        g.interior = 0;  // a storm outside soaks nothing while the hunter is indoors
        run(30.f);
        check(g.wet == 0.f, "a storm does not soak anything while the hunter is indoors");
        g.interior = -1;
        g.stormK = 0.f;
        check(wetGroundTile(T_GRASS) && wetGroundTile(T_STONE) && !wetGroundTile(T_SAND) && !wetGroundTile(T_MUD) && !wetGroundTile(T_SNOW) && !wetGroundTile(T_FLOORW),
              "rain darkens bare ground only: not sand, mud, snow or a floor indoors");
        // the wind in the sound: twice the wind, four times the energy (the desert bed has no birds or frogs to muddy it)
        double e1 = 0, e2 = 0;
        Audio w1, w2;
        w1.ambNow = w2.ambNow = AMB_DESERT;
        w2.windLvl = 2.f;
        for (int i = 0; i < 44100 * 2; i++) {
            const float a = w1.ambSample(), b = w2.ambSample();
            e1 += (double)a * a;
            e2 += (double)b * b;
        }
        check(e1 > 0 && e2 > e1 * 3.0, "the wind in the ambient bed follows the game's wind");
    }

    // ---- the rain sound (synthesised offline, no sound device)
    {
        Audio r;
        r.rainWant = 1.f;
        float peak = 0, tail = 0;
        bool finite = true;
        std::vector<float> buf(1024);
        for (int blk = 0; blk < 120; blk++) {  // ~2.8 s: past the fade-in
            Audio::cb(&r, (Uint8*)buf.data(), (int)(buf.size() * sizeof(float)));
            for (float v : buf) {
                finite = finite && std::isfinite(v);
                peak = std::max(peak, fabsf(v));
                if (blk >= 100) tail = std::max(tail, fabsf(v));
            }
        }
        check(finite && peak < 1.f, "the rain sound is finite and never clips");
        check(tail > 0.02f, "once faded in, the rain is clearly audible");
        Audio quiet;  // no rain wanted
        float qp = 0;
        for (int blk = 0; blk < 20; blk++) {
            Audio::cb(&quiet, (Uint8*)buf.data(), (int)(buf.size() * sizeof(float)));
            for (float v : buf) qp = std::max(qp, fabsf(v));
        }
        check(qp == 0.f, "no storm means no rain sound");
        Audio muted;
        muted.rainWant = 1.f;
        muted.muted = true;
        float mp = 0;
        for (int blk = 0; blk < 20; blk++) {
            Audio::cb(&muted, (Uint8*)buf.data(), (int)(buf.size() * sizeof(float)));
            for (float v : buf) mp = std::max(mp, fabsf(v));
        }
        check(mp == 0.f, "muted: the rain is silent too");
        Audio noDevice;
        noDevice.setRain(1.f);  // no audio device: must not touch it
        check(noDevice.rainWant == 0.f, "without a sound device setting the rain is a harmless no-op");
    }

    // ---- put everything back
    g.worldSeed = seed0;
    g.dayT = day0;
    g.stormK = storm0;
    g.galeK = gale0;
    g.wind = wind0;
    g.wet = wet0;
    g.windT = windT0;
    g.rainDrift = drift0;
    g.windGust = gust0;
    g.interior = interior0;
    g.delve = delve0;
    g.state = state0;
    g.wxKind = wx0;
    for (int b = 0; b < B_COUNT; b++) g.bw[b] = bw0[b];
}
