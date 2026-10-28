// audio.hpp - all sound effects are synthesised at start-up (no asset files needed).
#pragma once
#include <SDL.h>
#include <algorithm>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

enum Sfx { S_THROW, S_HIT, S_CRIT, S_KILL, S_PICKUP, S_COIN, S_GATHER, S_HURT, S_CLICK, S_CRAFT, S_ERROR, S_BREAK, S_CHOP, S_MINE, S_RUSTLE, S_FELL, S_SMELT, S_COUNT };

struct Audio {
    struct Voice {
        int snd;
        size_t pos;
        float vol;
    };
    SDL_AudioDeviceID dev = 0;
    bool ok = false;
    bool muted = false;
    std::vector<float> bank[S_COUNT];
    std::vector<Voice> voices;
    int rate = 44100;
    uint32_t seed = 12345;

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
        gen(S_BREAK, 0.16f, [&](float t, float u) { return noise() * expf(-t * 28.f) * (0.5f + 0.5f * sinf(t * 900.f)) * 0.7f; });
    }

    static void cb(void* ud, Uint8* stream, int len) {
        Audio* a = (Audio*)ud;
        float* out = (float*)stream;
        int n = len / (int)sizeof(float);
        for (int i = 0; i < n; i++) out[i] = 0;
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
        if (voices.size() < 24) voices.push_back({(int)s, 0, vol});
        SDL_UnlockAudioDevice(dev);
    }
    void shutdown() {
        if (dev) SDL_CloseAudioDevice(dev);
        dev = 0;
    }
};
