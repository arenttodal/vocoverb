// Reverb engines. Plate: original plate-style figure-eight tank following the published Dattorro topology
// (J. Dattorro, "Effect Design Part 1", JAES 1997) with stereo input, size scaling, low-band decay and an energy-
// guarded freeze. Wash: original 8/16-line modulated feedback delay network (orthogonal Hadamard matrix) with
// input diffusion and a bloom stage. Hall: original room-to-hall design in the spirit of the classic Lexicon
// halls (early-reflection pattern scaled by size, input diffusion, a ring of four modulated allpass/delay sections
// with smoothed random "wander", bass multiplier and decay-consistent damping). Shimmer: pitch-shifted feedback
// into any reverb's input (octave-up shimmer). All are 100% wet.
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

class HallReverb
{
public:
    static constexpr int kSections = 4, kEarly = 12;
    void prepare (double sampleRate);
    void reset() noexcept;
    void beginClear() noexcept { resetSmallState(); clears.begin(); }
    bool clearStep (int budget) noexcept { return clears.step (budget); }
    void process (const SpaceContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;

private:
    void resetSmallState() noexcept;
    struct Section
    {
        AllpassDelay ap1, ap2;
        DelayLine d1, d2;
        OnePoleLP damp, low;
        Drift wander;
        Lfo spin;
        float out = 0.0f;
    };
    double sr = 48000.0;
    DelayLine pre, preR, erL, erR;
    OnePoleLP bandL, bandR;
    DcBlocker hpL, hpR;
    AllpassDelay diffL[4], diffR[4], erApL, erApR;
    Section sec[kSections];
    float sizeSm = 0.55f;
    FreezeGuard guard;
    ClearList clears;
};

/** Shimmer: two-grain pitch shifter on the reverb output, band-limited and level-guarded, fed back to the input. */
class ShimmerUnit
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    /** Writes the next block's feedback (to add to the reverb input) from this block's reverb output. */
    void process (float amount, float semitones, const float* revL, const float* revR, float* fbL, float* fbR, int n) noexcept;

private:
    struct Shifter
    {
        DelayLine line;
        double phase = 0.0;
        float window = 2400.0f;
        float process (float x, float ratio) noexcept;
    };
    double sr = 48000.0;
    Shifter sh[2];
    Biquad hp[2], lp[2];
    float env = 0.0f, envA = 0.01f, envR = 0.001f, amountSm = 0.0f;
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
