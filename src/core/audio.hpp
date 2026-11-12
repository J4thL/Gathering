// audio.hpp - all sound effects are synthesised at start-up (no asset files needed).
#pragma once
#include <SDL.h>
#include <algorithm>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

enum Sfx { S_THROW, S_HIT, S_CRIT, S_KILL, S_PICKUP, S_COIN, S_GATHER, S_HURT, S_CLICK, S_CRAFT, S_ERROR, S_BREAK, S_CHOP, S_MINE, S_RUSTLE, S_FELL, S_SMELT,
           S_STEP_GRASS, S_STEP_STONE, S_STEP_WOOD, S_STEP_SAND, S_STEP_SNOW, S_STEP_MUD, S_COUNT };

// Ambient beds (one plays at a time, synthesised live in the audio callback, cross-faded when the scene changes)
enum Ambience { AMB_NONE, AMB_DAY, AMB_NIGHT, AMB_DESERT, AMB_SWAMP, AMB_SNOW, AMB_MOUNTAIN, AMB_CAVE, AMB_COUNT };

struct Audio {
    struct Voice {
        int snd;
        size_t pos;
        float vol;
    };
    SDL_AudioDeviceID dev = 0;
    bool ok = false;
    bool muted = false;
    float sfxVol = 1.f;  // settings: sound effects volume 0..1
    std::vector<float> bank[S_COUNT];
    std::vector<Voice> voices;
    int rate = 44100;
    uint32_t seed = 12345;
    // ---- ambience: the game sets the wanted bed + volume; the callback fades to it and synthesises it sample by sample
    int ambWant = AMB_NONE, ambNow = AMB_NONE;
    float ambVolWant = 0, ambGain = 0;
    uint32_t aSeed = 777;
    float aLp1 = 0, aLp2 = 0, aLfo = 0, aLfo2 = 0;
    float aEvT = 1.f, aEvAge = 99.f, aEvF = 3000.f, aEvPh = 0;  // the current one-shot voice (bird chirp, frog, drip) and the time to the next
    int aEvKind = 0;
    float aCri = 0;
    float aNoise() {
        aSeed ^= aSeed << 13;
        aSeed ^= aSeed >> 17;
        aSeed ^= aSeed << 5;
        return ((aSeed & 0xFFFF) / 32768.0f) - 1.0f;
    }
    float aRand() { return (aNoise() + 1.f) * 0.5f; }
    // one sample of the current ambient bed, at amplitude 1 (the caller scales and fades it)
    float ambSample() {
        const float PI2 = 6.2831853f, dt = 1.f / rate;
        aLfo += dt * 0.11f;
        aLfo2 += dt * 0.37f;
        const float gust = 0.55f + 0.45f * sinf(PI2 * aLfo) * (0.7f + 0.3f * sinf(PI2 * aLfo2));
        float windGain = 0.f, cut = 0.02f;
        switch (ambNow) {
            case AMB_DAY: windGain = 0.35f; cut = 0.025f; break;
            case AMB_NIGHT: windGain = 0.22f; cut = 0.018f; break;
            case AMB_DESERT: windGain = 0.5f; cut = 0.04f; break;
            case AMB_SWAMP: windGain = 0.16f; cut = 0.015f; break;
            case AMB_SNOW: windGain = 0.6f; cut = 0.07f; break;
            case AMB_MOUNTAIN: windGain = 0.55f; cut = 0.05f; break;
            case AMB_CAVE: windGain = 0.12f; cut = 0.006f; break;
            default: return 0.f;
        }
        aLp1 += (aNoise() - aLp1) * cut;
        aLp2 += (aLp1 - aLp2) * cut * 2.f;
        float v = aLp2 * windGain * gust * 6.f;
        // one-shot voices
        aEvT -= dt;
        if (aEvT <= 0 && aEvAge > 1.f) {
            aEvAge = 0;
            aEvPh = 0;
            switch (ambNow) {
                case AMB_DAY: case AMB_MOUNTAIN: aEvKind = 1; aEvF = 2400.f + 1800.f * aRand(); aEvT = 2.5f + 6.f * aRand(); break;       // a bird
                case AMB_SWAMP: aEvKind = 2; aEvF = 150.f + 70.f * aRand(); aEvT = 1.2f + 3.f * aRand(); break;                         // a frog
                case AMB_CAVE: aEvKind = 3; aEvF = 900.f + 700.f * aRand(); aEvT = 1.5f + 5.f * aRand(); break;                         // a drip
                default: aEvKind = 0; aEvT = 4.f; break;
            }
        }
        if (aEvAge <= 1.f) {
            aEvAge += dt;
            aEvPh += dt;
            if (aEvKind == 1) {  // two or three quick falling notes
                const float n = fmodf(aEvPh, 0.14f);
                if (aEvPh < 0.34f) v += sinf(PI2 * (aEvF - 700.f * n / 0.14f) * aEvPh) * sinf(3.1416f * n / 0.14f) * 0.22f;
            } else if (aEvKind == 2) {  // a croak: a low tone pulsing quickly
                if (aEvPh < 0.35f) v += sinf(PI2 * aEvF * aEvPh) * (0.5f + 0.5f * sinf(PI2 * 28.f * aEvPh)) * sinf(3.1416f * aEvPh / 0.35f) * 0.3f;
            } else if (aEvKind == 3) {  // a drip
                v += sinf(PI2 * aEvF * aEvPh) * expf(-aEvPh * 22.f) * 0.35f;
            }
            if (aEvAge > 0.5f) aEvAge = 99.f;
        }
        if (ambNow == AMB_NIGHT || ambNow == AMB_SWAMP) {  // crickets: a high tone chopped fast, swelling slowly
            aCri += dt;
            const float pulse = fmodf(aCri * 26.f, 1.f) < 0.45f ? 1.f : 0.f;
            v += sinf(PI2 * 4300.f * aCri) * pulse * (0.5f + 0.5f * sinf(PI2 * aLfo2 * 0.6f)) * (ambNow == AMB_NIGHT ? 0.07f : 0.04f);
        }
        if (ambNow == AMB_CAVE) v += sinf(PI2 * 55.f * aCri) * 0.03f, aCri += dt;
        return v;
    }

    float noise() {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return ((seed & 0xFFFF) / 32768.0f) - 1.0f;
    }

    void gen(int id, float dur, const std::function<float(float t, float u)>& fn) {
        int n = (int)(dur * rate);
        bank[id].resize(n);
        for (int i = 0; i < n; i++) {
            float t = (float)i / rate, u = (float)i / n;
            float v = fn(t, u);
            bank[id][i] = std::max(-1.f, std::min(1.f, v));
        }
    }

    void synth() {
        const float PI = 3.14159265f;
        float lp = 0;
        gen(S_THROW, 0.20f, [&](float t, float u) {
            lp += (noise() - lp) * (0.08f + u * 0.5f);
            return lp * sinf(PI * u) * 1.6f * 0.55f;
        });
        gen(S_HIT, 0.22f, [&](float t, float u) {
            float f = 170.f * expf(-t * 9.f) + 45.f;
            return (sinf(2 * PI * f * t) * expf(-t * 16.f) * 0.9f + noise() * expf(-t * 45.f) * 0.5f) * 0.8f;
        });
        gen(S_CRIT, 0.32f, [&](float t, float u) {
            float f = 170.f * expf(-t * 9.f) + 45.f;
            float ping = sinf(2 * PI * (1100.f - 500.f * u) * t) * expf(-t * 14.f) * 0.35f;
            return (sinf(2 * PI * f * t) * expf(-t * 14.f) * 0.9f + noise() * expf(-t * 40.f) * 0.5f) * 0.7f + ping;
        });
        gen(S_KILL, 0.42f, [&](float t, float u) {
            float f = 340.f * powf(1.f - u, 1.6f) + 70.f;
            return sinf(2 * PI * f * t) * (1.f - u) * 0.5f * (0.75f + 0.25f * sinf(t * 60.f)) + noise() * expf(-t * 30.f) * 0.2f;
        });
        gen(S_PICKUP, 0.11f, [&](float t, float u) { return sinf(2 * PI * (520.f + 280.f * u) * t) * (1.f - u) * 0.4f; });
        gen(S_COIN, 0.20f, [&](float t, float u) {
            float f = t < 0.07f ? 988.f : 1319.f;
            return (sinf(2 * PI * f * t) + 0.3f * sinf(4 * PI * f * t)) * expf(-(t < 0.07f ? t : t - 0.07f) * 22.f) * 0.32f;
        });
        float lp2 = 0;
        gen(S_GATHER, 0.15f, [&](float t, float u) {
            lp2 += (noise() - lp2) * 0.18f;
            return (lp2 * 1.4f + sinf(2 * PI * (110.f - 60.f * u) * t) * 0.5f) * expf(-t * 18.f) * 0.7f;
        });
        gen(S_HURT, 0.28f, [&](float t, float u) {
            float f = 240.f - 120.f * u;
            float saw = fmodf(f * t, 1.f) * 2.f - 1.f;
            return (saw * 0.45f + noise() * 0.2f) * (1.f - u);
        });
        gen(S_CLICK, 0.035f, [&](float t, float u) { return sinf(2 * PI * 760.f * t) * (1.f - u) * 0.3f; });
        gen(S_CRAFT, 0.36f, [&](float t, float u) {
            static const float notes[4] = {440.f, 554.f, 659.f, 880.f};
            int k = std::min(3, (int)(t / 0.075f));
            float lt = t - k * 0.075f;
            return sinf(2 * PI * notes[k] * t) * expf(-lt * 9.f) * 0.38f * (1.f - u * 0.5f);
        });
        gen(S_ERROR, 0.18f, [&](float t, float u) {
            float s = sinf(2 * PI * 140.f * t) > 0 ? 1.f : -1.f;
            return s * (1.f - u) * 0.22f;
        });
        // gathering: axe thunk, pickaxe clink, rustling plant, a tree coming down, a crackling fire
        float lp3 = 0;
        gen(S_CHOP, 0.15f, [&](float t, float u) {
            lp3 += (noise() - lp3) * 0.25f;
            return (sinf(2 * PI * (210.f - 110.f * u) * t) * expf(-t * 24.f) * 0.8f + lp3 * expf(-t * 55.f) * 0.5f) * 0.8f;
        });
        gen(S_MINE, 0.13f, [&](float t, float u) {
            return (sinf(2 * PI * 1480.f * t) * expf(-t * 38.f) * 0.30f + sinf(2 * PI * 2210.f * t) * expf(-t * 55.f) * 0.18f + noise() * expf(-t * 70.f) * 0.45f) * (1.f - u * 0.3f);
        });
        float lp4 = 0;
        gen(S_RUSTLE, 0.2f, [&](float t, float u) {
            lp4 += (noise() - lp4) * 0.3f;
            return lp4 * sinf(PI * u) * 0.75f * (0.7f + 0.3f * sinf(t * 90.f));
        });
        float lp5 = 0;
        gen(S_FELL, 0.5f, [&](float t, float u) {
            lp5 += (noise() - lp5) * 0.12f;
            float crack = (u < 0.25f ? noise() * expf(-fmodf(t, 0.05f) * 60.f) * 0.5f : 0.f);
            float thud = (u > 0.6f ? sinf(2 * PI * 62.f * (t - 0.3f)) * expf(-(t - 0.3f) * 10.f) * 0.9f : 0.f);
            return crack + thud + lp5 * (1.f - u) * 0.35f;
        });
        float lp6 = 0;
        gen(S_SMELT, 0.3f, [&](float t, float u) {
            lp6 += (noise() - lp6) * 0.5f;
            return (lp6 * expf(-t * 12.f) * 0.5f + sinf(2 * PI * (420.f + 520.f * u) * t) * (1.f - u) * 0.18f);
        });
        {  // footsteps: a soft thump + a burst of filtered noise, different for each surface
            float f1 = 0, f2 = 0, f3 = 0, f4 = 0, f5 = 0, f6 = 0;
            gen(S_STEP_GRASS, 0.09f, [&](float t, float u) { f1 += (noise() - f1) * 0.25f; return f1 * expf(-t * 40.f) * 0.9f + sinf(2 * PI * 90.f * t) * expf(-t * 50.f) * 0.2f; });
            gen(S_STEP_STONE, 0.08f, [&](float t, float u) { f2 += (noise() - f2) * 0.7f; return f2 * expf(-t * 70.f) * 0.55f + sinf(2 * PI * 420.f * t) * expf(-t * 90.f) * 0.18f; });
            gen(S_STEP_WOOD, 0.1f, [&](float t, float u) { f3 += (noise() - f3) * 0.35f; return sinf(2 * PI * 150.f * t) * expf(-t * 45.f) * 0.45f + f3 * expf(-t * 60.f) * 0.3f; });
            gen(S_STEP_SAND, 0.13f, [&](float t, float u) { f4 += (noise() - f4) * 0.45f; return f4 * sinf(PI * std::min(1.f, u * 1.5f)) * expf(-t * 18.f) * 0.5f; });
            gen(S_STEP_SNOW, 0.12f, [&](float t, float u) { f5 += (noise() - f5) * 0.55f; return f5 * (0.6f + 0.4f * sinf(t * 700.f)) * expf(-t * 22.f) * 0.55f; });
            gen(S_STEP_MUD, 0.14f, [&](float t, float u) { f6 += (noise() - f6) * 0.12f; return f6 * expf(-t * 16.f) * 1.1f + sinf(2 * PI * (130.f - 400.f * t) * t) * expf(-t * 30.f) * 0.25f; });
        }
        gen(S_BREAK, 0.16f, [&](float t, float u) { return noise() * expf(-t * 28.f) * (0.5f + 0.5f * sinf(t * 900.f)) * 0.7f; });
    }

    static void cb(void* ud, Uint8* stream, int len) {
        Audio* a = (Audio*)ud;
        float* out = (float*)stream;
        int n = len / (int)sizeof(float);
        for (int i = 0; i < n; i++) out[i] = 0;
        for (int i = 0; i < n; i++) {  // the ambient bed: fade out, switch, fade in
            const float want = a->muted || a->ambWant != a->ambNow ? 0.f : a->ambVolWant;
            a->ambGain += (want - a->ambGain) * 0.00008f;
            if (a->ambWant != a->ambNow && a->ambGain < 0.004f) a->ambNow = a->ambWant, a->aEvAge = 99.f, a->aEvT = 1.f;
            if (a->ambNow != AMB_NONE && a->ambGain > 0.0005f) out[i] += a->ambSample() * a->ambGain * 0.5f;
        }
        for (auto& v : a->voices) {
            const auto& b = a->bank[v.snd];
            for (int i = 0; i < n && v.pos < b.size(); i++, v.pos++) out[i] += b[v.pos] * v.vol;
        }
        for (int i = 0; i < n; i++) out[i] = std::max(-1.f, std::min(1.f, out[i]));
        a->voices.erase(std::remove_if(a->voices.begin(), a->voices.end(), [&](const Voice& v) { return v.pos >= a->bank[v.snd].size(); }),
                        a->voices.end());
    }

    void init() {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
        synth();
        SDL_AudioSpec want{}, have{};
        want.freq = rate;
        want.format = AUDIO_F32SYS;
        want.channels = 1;
        want.samples = 1024;
        want.callback = &Audio::cb;
        want.userdata = this;
        dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (!dev) return;
        ok = true;
        SDL_PauseAudioDevice(dev, 0);
    }
    void play(Sfx s, float vol = 0.7f, float pitchJitter = 0) {
        (void)pitchJitter;
        if (!ok || muted) return;
        SDL_LockAudioDevice(dev);
        if (voices.size() < 24) voices.push_back({(int)s, 0, vol * sfxVol});
        SDL_UnlockAudioDevice(dev);
    }
    // choose the ambient bed (AMB_*) and its volume (0..1); the callback cross-fades to it
    void setAmbience(int kind, float vol) {
        if (!ok || (kind == ambWant && vol == ambVolWant)) return;
        SDL_LockAudioDevice(dev);
        ambWant = kind;
        ambVolWant = vol;
        SDL_UnlockAudioDevice(dev);
    }
    void shutdown() {
        if (dev) SDL_CloseAudioDevice(dev);
        dev = 0;
    }
};
