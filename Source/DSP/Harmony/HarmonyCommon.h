// Shared harmony-method interface and carrier synthesis.
#pragma once

#include "../../Core/DspUtil.h"
#include "../../Core/Notes.h"
#include "../../Core/Params.h"

namespace pa
{
/** Per-chunk context handed to every harmony method. Arrays have n <= VoiceBank::kChunk samples. */
struct HarmonyContext
{
    const ParamSet* p = nullptr;
    const VoiceBank* voices = nullptr;
    const float* carrier = nullptr;     // mono band-limited carrier (already enveloped & normalised)
    const float* carrierGain = nullptr; // sum of voice gains per sample (0 when no voice sounds)
    int quality = 1;                    // 0 Eco, 1 Standard, 2 High
    double sr = 48000.0;
};

/** Polyphonic band-limited carrier oscillator bank (PolyBLEP saw/pulse) driven by the VoiceBank. */
class CarrierSynth
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); }
    void reset() noexcept
    {
        for (int v = 0; v < kMaxVoices; ++v) { ph[v][0] = 0.13 * v; ph[v][1] = 0.61 + 0.07 * v; }
        tilt.reset(); tilt2.reset();
    }
    /** Renders the mono carrier and the per-sample gain sum. */
    void render (const VoiceBank& vb, const ParamSet& p, int n, float* out, float* gainSum) noexcept;

private:
    static inline double blep (double t, double dt) noexcept
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }
    double sr = 48000.0;
    double ph[kMaxVoices][2] {};
    OnePoleLP tilt, tilt2;
};

inline void CarrierSynth::render (const VoiceBank& vb, const ParamSet& p, int n, float* out, float* gainSum) noexcept
{
    const int shape = p.i (ClCarrier);
    const float detuneCents = p[ClDetune];
    const bool detune = detuneCents > 0.5f;
    const double dRatio = std::exp2 (detuneCents / 1200.0);
    const float colour = p[Colour] / 100.0f;
    const double cutoff = 700.0 * std::exp2 (colour * 4.8); // 0.7 .. ~19.5 kHz
    tilt.setCutoff (sr, cutoff);
    tilt2.setCutoff (sr, cutoff * 1.6);
    const double nyq = sr * 0.5;

    for (int i = 0; i < n; ++i)
    {
        double acc = 0.0;
        float gs = 0.0f;
        for (int v = 0; v < kMaxVoices; ++v)
        {
            const float g = vb.gain[v][i];
            gs += g;
            if (g <= 1.0e-7f) continue;
            const double f = vb.hz[v][i];
            if (f <= 0.0 || f >= nyq) continue;
            const int nOsc = detune ? 2 : 1;
            double vs = 0.0;
            for (int o = 0; o < nOsc; ++o)
            {
                const double fo = o == 0 ? (detune ? f / std::sqrt (dRatio) : f) : f * std::sqrt (dRatio);
                const double dt = fo / sr;
                double& t = ph[v][o];
                double s;
                if (shape == 2)
                {
                    s = (t < 0.5 ? 1.0 : -1.0) + blep (t, dt) - blep (std::fmod (t + 0.5, 1.0), dt);
                    s *= 0.8;
                }
                else
                {
                    s = 2.0 * t - 1.0 - blep (t, dt);
                    if (shape == 1)
                    {
                        const double pw = 0.22;
                        const double pls = (t < pw ? 1.0 : -1.0) + blep (t, dt) - blep (std::fmod (t + 1.0 - pw, 1.0), dt);
                        s = 0.6 * s + 0.5 * pls;
                    }
                }
                vs += s;
                t += dt;
                if (t >= 1.0) t -= 1.0;
            }
            acc += vs * (detune ? 0.7071 : 1.0) * g;
        }
        float y = (float) acc;
        y = shape == 1 ? tilt2.process (y) : tilt.process (y);
        out[i] = y;
        gainSum[i] = gs;
    }
}

/** Integer-sample stereo delay used for latency padding and ordinary-path alignment. */
struct StereoDelay
{
    DelayLine l, r;
    void allocate (int maxSamples) { l.allocate (maxSamples + 2); r.allocate (maxSamples + 2); }
    void clear() noexcept { l.clear(); r.clear(); }
    inline void push (float a, float b) noexcept { l.push (a); r.push (b); }
    /** Read delay d >= 0 samples relative to the sample just pushed (0 = that sample). */
    inline float readL (int d) const noexcept { return l.readInt (d + 1); }
    inline float readR (int d) const noexcept { return r.readInt (d + 1); }
};

} // namespace pa
