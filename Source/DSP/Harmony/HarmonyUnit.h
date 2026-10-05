// One harmony processing state: hosts the four methods, crossfades method changes, aligns latency and blends
// ordinary wet with harmonised wet:  out = (1-d) * align(in) + d * H(in).
#pragma once

#include "ClassicVocoder.h"
#include "FftVocoder.h"
#include "IntervalShift.h"
#include "Resonator.h"

namespace pa
{
enum Method { MethodOff = 0, MethodClassic = 1, MethodFft = 2, MethodResonator = 3, MethodShift = 4 };

class HarmonyUnit
{
public:
    void prepare (double sampleRate, int maxLatencySamples);
    void reset() noexcept;
    /** dStart/dEnd: harmony depth ramp across the chunk. unitLatency: total alignment (Studio = fixed max). */
    void process (const HarmonyContext& ctx, int method, bool studio, int studioLatency,
                  const float* inL, const float* inR, float* outL, float* outR, int n, float dStart, float dEnd) noexcept;
    int methodLatency (int method, int quality) const noexcept;
    /** Current output alignment in samples (what this unit adds to the wet path). */
    int currentLatency() const noexcept { return lastLatency; }
    int activeMethod() const noexcept { return cur; }
    void setInitialMethod (int m) noexcept { cur = m; prev = MethodOff; fadePos = fadeLen; }
    bool isFading() const noexcept { return fadePos < fadeLen; }

    ClassicVocoder classic;
    FftVocoder fftVoc;
    Resonator resonator;
    IntervalShift shift;

private:
    void runMethod (int m, const HarmonyContext& ctx, const float* inL, const float* inR, float* oL, float* oR, int n) noexcept;
    void resetMethod (int m) noexcept;
    double sr = 48000.0;
    int cur = MethodClassic, prev = MethodOff;
    int fadePos = 0, fadeLen = 1;
    int lastLatency = 0, lastQuality = 1;
    bool idle = false;
    StereoDelay ord, padCur, padPrev;
    float aL[VoiceBank::kChunk], aR[VoiceBank::kChunk], bL[VoiceBank::kChunk], bR[VoiceBank::kChunk];
};
} // namespace pa
