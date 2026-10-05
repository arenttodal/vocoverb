// STFT / weighted-overlap-add vocoder. Smoothed modulator spectral envelopes are imposed on the spectrum of the
// MIDI carrier (carrier phase retained). sqrt-Hann analysis + synthesis windows, 75% overlap (hop = N/4).
#pragma once

#include "../../Core/FFT.h"
#include "HarmonyCommon.h"

#include <complex>
#include <vector>

namespace pa
{
class FftVocoder
{
public:
    void prepare (double sampleRate, int maxFftSize);
    void reset() noexcept;
    void process (const HarmonyContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;
    /** Buffering latency: N (WOLA) + N/4 (frame work is spread over the following hop to keep block cost flat). */
    int latencySamples() const noexcept { return fftSize + fftSize / 4; }
    static int latencyForSize (int n) noexcept { return n + n / 4; }
    static int sizeForQuality (int quality, double sr) noexcept
    {
        const int base = quality == 0 ? 1024 : (quality == 1 ? 2048 : 4096);
        return sr > 70000.0 ? base * 2 : base;
    }
    int currentSize() const noexcept { return fftSize; }
    /** Test hook: pass the modulator spectrum straight through (verifies WOLA reconstruction & latency). */
    bool identityMode = false;

private:
    void configure (int n) noexcept;
    void beginFrame() noexcept;
    void runStage (const HarmonyContext& ctx) noexcept;
    void finishFrame() noexcept;
    static constexpr int kStages = 6;
    int stage = 0, minHalfC = 2, tiltN = 0;
    double meanEnvC = 0.0;
    float tiltKey = -100.0f;
    std::vector<float> tiltTab;
    double sr = 48000.0;
    int maxN = 0, fftSize = 0, hop = 0, fill = 0;
    FFT fft;
    std::vector<float> win, inL, inR, inC, accL, accR, readyL, readyR;
    std::vector<std::complex<float>> zMod, zCar, specL, specR;
    std::vector<float> envL, envR, envC, prevL, prevR;
    std::vector<double> csL, csR, csC;
    float gainAccum = 0.0f, frameGain = 0.0f;
    int readPos = 0;
};
} // namespace pa
