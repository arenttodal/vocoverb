// Interval Shift: up to six relative transpositions of the designated audio (dual-window delay-line shifter).
// Shift per voice = played note - Shift Reference. It transposes the whole input; it does not re-tune individual
// notes inside a polyphonic tail. Unity ratio uses a true identity (pure delay) path.
#pragma once

#include "HarmonyCommon.h"

namespace pa
{
class IntervalShift
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void process (const HarmonyContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;
    int latencySamples() const noexcept { return latency; }
    static float windowMsForQuality (int q) noexcept { return q == 0 ? 40.0f : (q == 1 ? 60.0f : 80.0f); }
    int latencyForQuality (int q) const noexcept
    {
        int w = std::max (64, (int) std::round (windowMsForQuality (q) * 0.001 * sr));
        if (w & 1) ++w;
        return 2 + w / 2;
    }

private:
    void configure (int quality) noexcept;
    static constexpr int kBands = 16;
    double sr = 48000.0;
    DelayLine bufL, bufR;
    int quality = -1, winLen = 2880, latency = 1442, dmin = 2;
    struct Voice { double phase = 0.0; float idW = 1.0f; float gl = 1, gr = 1; };
    Voice voice[kMaxVoices];
    OnePoleLP toneL, toneR;
    // approximate formant preservation (band envelope matching)
    Biquad refBpL[kBands], refBpR[kBands], outBpL[kBands], outBpR[kBands];
    float envRefL[kBands] {}, envRefR[kBands] {}, envOutL[kBands] {}, envOutR[kBands] {}, gainL[kBands] {}, gainR[kBands] {};
    float envCoef = 0.01f, gainCoef = 0.001f;
};
} // namespace pa
