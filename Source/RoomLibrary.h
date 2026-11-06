#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

// Built-in spaces. These are procedurally generated impulse responses
// (early reflections + frequency-dependent decaying diffuse tail).
// For recorded spaces, use the "Load IR..." button (any WAV/AIFF/FLAC IR).
struct RoomSpec
{
    const char* name;
    float rt60;          // seconds
    float preDelayMs;
    int   earlyCount;    // number of discrete early reflections
    float earlyMs;       // time window for early reflections
    float brightStart;   // 0..1 lowpass coefficient at tail start (1 = bright)
    float brightEnd;     // ... at tail end (lower = darker decay)
};

inline const std::array<RoomSpec, 8>& roomSpecs()
{
    static const std::array<RoomSpec, 8> specs {{
        { "Small Room",    0.45f,  2.f, 12,  20.f, 0.70f, 0.25f },
        { "Live Room",     0.90f,  4.f, 16,  35.f, 0.60f, 0.20f },
        { "Wooden Hall",   1.80f,  8.f, 24,  60.f, 0.55f, 0.15f },
        { "Concert Hall",  2.60f, 15.f, 28,  80.f, 0.50f, 0.12f },
        { "Church",        3.80f, 20.f, 30, 100.f, 0.45f, 0.10f },
        { "Cathedral",     6.00f, 30.f, 34, 140.f, 0.40f, 0.08f },
        { "Cave",          7.50f, 40.f, 20, 180.f, 0.30f, 0.05f },
        { "Arena",         4.50f, 45.f, 20, 160.f, 0.50f, 0.10f }
    }};
    return specs;
}

inline juce::AudioBuffer<float> makeIR (const RoomSpec& s, double sr, int seed)
{
    const int len = (int) (sr * s.rt60 * 1.1);
    juce::AudioBuffer<float> ir (2, len);
    ir.clear();

    for (int ch = 0; ch < 2; ++ch)
    {
        juce::Random rng (seed * 131 + ch * 7919 + 17);
        auto* d = ir.getWritePointer (ch);
        const int pre = (int) (s.preDelayMs * 0.001 * sr);
        float lp = 0.f;

        for (int i = pre; i < len; ++i)
        {
            const float t     = (float) (i - pre) / (float) sr;
            const float env   = std::exp (-6.9078f * t / s.rt60);
            const float onset = 1.f - std::exp (-t / 0.012f);
            const float a     = juce::jlimit (0.01f, 1.f,
                                  juce::jmap (t / s.rt60, s.brightStart, s.brightEnd));
            const float noise = rng.nextFloat() * 2.f - 1.f;
            lp += a * (noise - lp);
            d[i] = lp * env * onset;
        }

        for (int k = 0; k < s.earlyCount; ++k)
        {
            const float tms = 1.f + s.earlyMs * std::pow (rng.nextFloat(), 1.5f);
            const int idx = pre + (int) (tms * 0.001 * sr);
            if (idx < len)
            {
                const float amp = 0.8f * std::exp (-tms / (s.earlyMs * 0.8f));
                d[idx] += (rng.nextBool() ? amp : -amp);
            }
        }

        const int fade = len / 20;
        for (int i = 0; i < fade; ++i)
            d[len - 1 - i] *= (float) i / (float) fade;
    }
    return ir;
}
