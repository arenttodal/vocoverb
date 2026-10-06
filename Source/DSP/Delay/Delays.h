// Delay engines: BBD-character echo, Interval (pitch-shaped taps) delay, and a crossfaded capture looper used
// for delay freeze. All are fully wet (no direct signal) and preallocated in prepare().
#pragma once

#include "../../Core/DspUtil.h"
#include "../../Core/Notes.h"
#include "../../Core/Params.h"

namespace pa
{
struct SpaceContext
{
    const ParamSet* p = nullptr;
    double bpm = 120.0;
    float pitchDriverSemis = 0.0f; // Interval delay MIDI/Arp relative offset
    bool freeze = false;           // engine-level freeze (reverbs); delay freeze uses CaptureLooper
    float overdub = 0.0f;
    float motionMod = 0.0f;        // mod-wheel contribution to Motion (0..1)
    int quality = 1;
};

/** Progressive clear helper shared by engines (bounded work per call). */
struct ClearList
{
    DelayLine* lines[48] {};
    int count = 0, idx = 0;
    bool active = false;
    void add (DelayLine& d) noexcept { if (count < 48) lines[count++] = &d; }
    void begin() noexcept { idx = 0; active = true; for (int i = 0; i < count; ++i) lines[i]->beginProgressiveClear(); }
    bool step (int budget) noexcept
    {
        if (! active) return true;
        while (idx < count && budget > 0)
        {
            if (lines[idx]->clearStep (budget)) ++idx;
            budget = 0;
        }
        if (idx >= count) active = false;
        return ! active;
    }
};

class BbdDelay
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void beginClear() noexcept { resetSmallState(); clears.begin(); }
    bool clearStep (int budget) noexcept { return clears.step (budget); }
    void process (const SpaceContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;
    float currentTimeMs() const noexcept { return timeMs; }

private:
    void resetSmallState() noexcept;
    double sr = 48000.0;
    DelayLine lineL, lineR;
    ClearList clears;
    float timeMs = 375.0f, headA = 375.0f, headB = 375.0f, xfade = 1.0f, xfadeStep = 0.0f;
    bool xfading = false;
    Biquad inLpL, inLpR, fbLpL[2], fbLpR[2];
    DcBlocker hpL, hpR;
    Lfo wow, flutter;
    Drift drift;
    Rng noiseRng { 777u };
    float envWL = 0, envWR = 0, envRL = 0, envRR = 0, envN = 0;
    float yL1 = 0, yR1 = 0;
    float lastTone = -1, lastAge = -1;
    float envA = 0.01f, envRel = 0.001f, envRelSlow = 0.0005f;
    bool primed = false; // first block after reset starts at the set time (no glide / crossfade from a stale value)
};

class IntervalDelay
{
public:
    static constexpr int kTaps = 3;
    void prepare (double sampleRate);
    void reset() noexcept;
    void beginClear() noexcept { resetSmallState(); clears.begin(); }
    bool clearStep (int budget) noexcept { return clears.step (budget); }
    void process (const SpaceContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;
    float currentTimeMs() const noexcept { return timeMs; }
    /** Shifted-tap latency information (Stable forward reads centre on the nominal time). */
    float reverseAcquisitionMs (const ParamSet& p) const noexcept;

private:
    void resetSmallState() noexcept;
    struct Grain { double u = 0.0; double start = 0.0; float jitter = 0.0f; bool active = false; double t = 0.0; double dur = 1.0; double fade = 1.0; int dir = 1; float rate = 1.0f; };
    struct Tap
    {
        DelayLine aaL, aaR;          // anti-aliased copy of the written signal for this tap's rate
        Biquad aaFiltL[2], aaFiltR[2];
        float aaCut = -1.0f;
        float ratio = 1.0f, ratioTarget = 1.0f;
        double phase = 0.0;          // stable forward shifter phase
        double revU = 0.0;           // stable reverse grain phase
        float jit[2] {};
        float idW = 1.0f;
        Grain frag[2];               // clock fragments
        int fragNext = 0;
        float gl = 1, gr = 1, level = 1;
    };
    float readTap (const Tap& t, bool left, double delay) const noexcept;
    double sr = 48000.0;
    DelayLine mainL, mainR;
    Tap taps[kTaps];
    ClearList clears;
    long long written = 0;
    float timeMs = 500.0f, timeSmoothed = 500.0f;
    Biquad fbLpL, fbLpR;
    DcBlocker fbHpL, fbHpR;
    AllpassDelay fbApL[2], fbApR[2], outApL[2], outApR[2];
    Rng rng { 4242u };
    int maxDelaySamples = 0;
};

/** Tape echo in the spirit of the classic multi-head machines: one tape loop, three playback heads at 1x/2x/3x of the
    head-1 time, selectable head combinations, record saturation, wow and flutter that scale with head distance,
    playback head bump and high-frequency loss, a motor glide on time changes and optional hiss. */
class TapeDelay
{
public:
    static constexpr int kHeads = 3;
    void prepare (double sampleRate);
    void reset() noexcept;
    void beginClear() noexcept { resetSmallState(); clears.begin(); }
    bool clearStep (int budget) noexcept { return clears.step (budget); }
    void process (const SpaceContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;
    float currentTimeMs() const noexcept { return timeMs; }
    /** Head mask (bit k = head k+1) for a TpHeads choice. */
    static int headMask (int choice) noexcept;

private:
    void resetSmallState() noexcept;
    double sr = 48000.0;
    DelayLine tape;
    ClearList clears;
    float timeMs = 320.0f, headMs = 320.0f;
    Biquad bump[kHeads], loss[kHeads], loss2[kHeads];
    DcBlocker hp;
    Lfo wow, flutter;
    Drift drift;
    Rng hissRng { 31337u };
    float env = 0.0f, hissEnv = 0.0f, fbState = 0.0f;
    float lastTone = -1.0f;
    bool primed = false; // first block after reset jumps to the set time (no glide from a stale value)
};

/** Records the recent wet output of an engine; on freeze, loops a bounded region with crossfaded boundaries. */
class CaptureLooper
{
public:
    void prepare (double sampleRate, double maxSeconds);
    void reset() noexcept;
    void beginClear() noexcept { recorded = 0; frozen = false; mix = 0.0f; clears.begin(); }
    bool clearStep (int budget) noexcept { return clears.step (budget); }
    /** in: engine output; returns looped/blended output into out. loopSeconds chosen at engage. */
    void process (bool freeze, float loopSeconds, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;
    bool isFrozen() const noexcept { return frozen; }

private:
    double sr = 48000.0;
    DelayLine bufL, bufR;
    ClearList clears;
    long long recorded = 0;
    bool frozen = false;
    int loopLen = 0, xf = 0, pos = 0;
    int regionStart = 0; // delay (from write head) of region start at freeze time
    float mix = 0.0f, mixStep = 0.0f;
};

double syncedMs (int divIndex, double bpm) noexcept;

} // namespace pa
