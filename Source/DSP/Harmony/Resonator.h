// Tuned resonator: the wet audio excites banks of note-tuned complex one-pole resonators (coupled/rotational form,
// energy-safe under retuning). MIDI sets the frequencies; the audio supplies all energy.
#pragma once

#include "HarmonyCommon.h"

namespace pa
{
class Resonator
{
public:
    static constexpr int kMaxPartials = 16;
    void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); }
    void reset() noexcept;
    void process (const HarmonyContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;
    int latencySamples() const noexcept { return 0; }
    static int partialCap (int quality) noexcept { return quality == 0 ? 6 : (quality == 1 ? 12 : 16); }

private:
    struct Partial
    {
        float pr = 0, pi = 0, b = 0;
        float lr = 0, li = 0, rr = 0, ri = 0;
        float gl = 1, gr = 1;
    };
    struct Voice
    {
        Partial part[kMaxPartials];
        int count = 0;
        float coefHz = -1.0f;
        float x1L = 0, x1R = 0;
    };
    void updateCoefs (Voice& v, float f0, const ParamSet& p, int cap) noexcept;
    double sr = 48000.0;
    Voice voice[kMaxVoices];
    float paramKey = -1.0f;
};
} // namespace pa
