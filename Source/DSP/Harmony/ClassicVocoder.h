// Classic filter-bank vocoder: log-spaced analysis bands on the (wet) modulator, envelopes imposed on the
// matching bands of a MIDI-driven carrier. Carrier band energy is normalised (bounded) so the modulator's
// spectral envelope - not the carrier's natural tilt - shapes the output.
#pragma once

#include "HarmonyCommon.h"

namespace pa
{
class ClassicVocoder
{
public:
    static constexpr int kMaxBands = 48;
    void prepare (double sampleRate);
    void reset() noexcept;
    void process (const HarmonyContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;
    int latencySamples() const noexcept { return 0; }
    int numBands() const noexcept { return bands; }
    float bandFrequency (int b) const noexcept { return fc[b]; }

private:
    void configure (int numBands, int order, const ParamSet& p) noexcept;
    struct Band
    {
        Biquad mL[2], mR[2], c[2];
        float envL = 0, envR = 0, envC = 0, envG = 0;
    };
    double sr = 48000.0;
    int bands = 0, order = 2;
    float fc[kMaxBands] {};
    float tiltGain[kMaxBands] {};
    float lastColour = -1.0f, lastAttack = -1, lastRelease = -1;
    float atk = 0.1f, rel = 0.01f, catk = 0.1f, crel = 0.01f;
    float bwOct = 0.15f;
    Band band[kMaxBands];
    float envBufL[kMaxBands][VoiceBank::kChunk] {};
    float envBufR[kMaxBands][VoiceBank::kChunk] {};
    Biquad noiseHpL[2], noiseHpR[2];
    float calib = 1.0f;
};
} // namespace pa
