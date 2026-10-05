// Reverb engines. Plate: original plate-style figure-eight tank following the published Dattorro topology
// (J. Dattorro, "Effect Design Part 1", JAES 1997) with stereo input, size scaling, low-band decay and an energy-
// guarded freeze. Wash: original 8/16-line modulated feedback delay network (orthogonal Hadamard matrix) with
// input diffusion and a bloom stage. Both are 100% wet.
#pragma once

#include "../Delay/Delays.h"

namespace pa
{
/** Shared freeze/energy guard logic. */
struct FreezeGuard
{
    float ms = 0.0f, ref = 0.0f, msCoef = 0.0005f;
    int armCounter = 0;
    bool wasFrozen = false;
    float inputGain = 1.0f, inputStep = 0.001f;
    void prepare (double sr) noexcept { msCoef = onePoleCoef (0.25f, sr); inputStep = 1.0f / (0.05f * (float) sr); }
    /** Update per sample with a tank energy proxy; returns extra loop-gain multiplier (<= 1). */
    inline float update (bool frozen, float energy, int armSamples) noexcept
    {
        ms += msCoef * (energy - ms);
        if (frozen && ! wasFrozen) { armCounter = armSamples; ref = 0.0f; }
        wasFrozen = frozen;
        if (! frozen) return 1.0f;
        if (armCounter > 0) { if (--armCounter == 0) ref = ms; return 1.0f; }
        if (ref > 1.0e-12f && ms > ref * 1.4f) return 0.9995f;
        return 1.0f;
    }
    inline float nextInputGain (bool frozen, float overdub) noexcept
    {
        const float target = frozen ? overdub : 1.0f;
        if (inputGain < target) inputGain = std::min (target, inputGain + inputStep);
        else if (inputGain > target) inputGain = std::max (target, inputGain - inputStep);
        return inputGain;
    }
};

class PlateReverb
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void beginClear() noexcept { resetSmallState(); clears.begin(); }
    bool clearStep (int budget) noexcept { return clears.step (budget); }
    void process (const SpaceContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;

private:
    void resetSmallState() noexcept;
    struct Half
    {
        AllpassDelay apMod, ap2;
        DelayLine d1, d2;
        OnePoleLP damp, lowSplit1, lowSplit2;
    };
    double sr = 48000.0, scale = 1.0;
    DelayLine pre, preR;
    OnePoleLP bandL, bandR;
    AllpassDelay diffL[4], diffR[4];
    Half A, B;
    Lfo lfoA, lfoB;
    Drift drift;
    float sizeSm = 1.0f, fbA = 0.0f, fbB = 0.0f;
    FreezeGuard guard;
    ClearList clears;
};

class WashReverb
{
public:
    static constexpr int kMaxLines = 16;
    void prepare (double sampleRate);
    void reset() noexcept;
    void beginClear() noexcept { resetSmallState(); clears.begin(); }
    bool clearStep (int budget) noexcept { return clears.step (budget); }
    void process (const SpaceContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;
    int numLines() const noexcept { return lines; }

private:
    void resetSmallState() noexcept;
    double sr = 48000.0;
    int lines = 16;
    DelayLine line[kMaxLines];
    OnePoleLP damp[kMaxLines], lowSplit[kMaxLines];
    Lfo lfo[kMaxLines];
    float baseLen[kMaxLines] {};
    AllpassDelay diffL[4], diffR[4], bloomL[4], bloomR[4];
    float sizeSm = 1.0f;
    FreezeGuard guard;
    ClearList clears;
    float y[kMaxLines] {};
};
} // namespace pa
