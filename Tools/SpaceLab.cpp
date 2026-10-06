// pa_spacelab: objective measurements of the delay and reverb engines through the full engine (wet only,
// harmony off, no duck, wet filters open). Used to find and track flaws; prints one line per configuration.
//   pa_spacelab [--sr 48000] [--wav DIR]
// Reverb metrics (impulse response): T30 broadband and per octave (125 Hz / 1 kHz / 4 kHz) from Schroeder
// integration, IR energy (dB, loudness consistency across modes), time to echo density 0.9 (normalised echo
// density, Abel & Huang), late-tail spectral peakiness (max/median of 1/12-octave smoothed magnitude, dB; high =
// metallic ringing), interchannel correlation of the late tail, DC, CPU (x realtime).
// Delay metrics: first repeat time, repeat-to-repeat decay, peak at maximum feedback (stability).
#include "Core/Analysis.h"
#include "Core/Engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace pa;

namespace
{
double SR = 48000.0;
std::string wavDir;

struct Buf { std::vector<float> L, R; };

void writeWav (const std::string& path, const Buf& b)
{
    std::ofstream f (path, std::ios::binary);
    const uint32_t n = (uint32_t) b.L.size(), bytes = n * 8;
    auto u32 = [&f] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&f] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); u32 (36 + bytes); f.write ("WAVE", 4);
    f.write ("fmt ", 4); u32 (16); u16 (3); u16 (2); u32 ((uint32_t) SR); u32 ((uint32_t) SR * 8); u16 (8); u16 (32);
    f.write ("data", 4); u32 (bytes);
    for (uint32_t i = 0; i < n; ++i) { f.write ((const char*) &b.L[i], 4); f.write ((const char*) &b.R[i], 4); }
}

ParamSet base()
{
    ParamSet p;
    p[DelayEnable] = 0; p[ReverbEnable] = 0;
    p[HarmEnable] = 0; p[HarmMethod] = 1; p[DuckAmount] = 0;
    p[Timing] = 0; p[WetLevel] = 0; p[DryLevel] = -60; p[WetOnly] = 1; p[Mix] = 100;
    p[WetLowCut] = 20; p[WetHighCut] = 20000; p[Width] = 100;
    return p;
}

Buf run (const ParamSet& p, const Buf& in, double* cpuX = nullptr)
{
    auto eng = std::make_unique<Engine>();
    const int block = 256;
    eng->prepare (SR, block);
    Buf out; out.L.assign (in.L.size(), 0.0f); out.R.assign (in.L.size(), 0.0f);
    TransportInfo tp;
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t pos = 0; pos < in.L.size(); pos += block)
    {
        const int n = (int) std::min<size_t> (block, in.L.size() - pos);
        eng->process (p, tp, in.L.data() + pos, in.R.data() + pos, out.L.data() + pos, out.R.data() + pos, n, nullptr, 0);
    }
    const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    if (cpuX) *cpuX = ((double) in.L.size() / SR) / std::max (1e-9, sec);
    return out;
}

Buf impulse (double sec, float amp = 0.5f)
{
    Buf b; b.L.assign ((size_t) (sec * SR), 0.0f); b.R = b.L;
    const size_t at = (size_t) (0.05 * SR);
    b.L[at] = amp; b.R[at] = amp;
    return b;
}

std::vector<float> bandFilter (const std::vector<float>& x, double fc)
{
    Biquad a, b; a.bandpass (SR, fc, 1.41); b.bandpass (SR, fc, 1.41);
    std::vector<float> y (x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = b.process (a.process (x[i]));
    return y;
}

/** T30 (s) from Schroeder backward integration: fit between -5 and -35 dB (falls back to T20). */
double t30 (const std::vector<float>& x, size_t start)
{
    const size_t n = x.size();
    std::vector<double> e (n, 0.0);
    double acc = 0.0;
    for (size_t i = n; i-- > start;) { acc += (double) x[i] * x[i]; e[i] = acc; }
    if (acc <= 0.0) return 0.0;
    auto lvl = [&] (size_t i) { return 10.0 * std::log10 (std::max (1e-30, e[i] / acc)); };
    size_t i5 = start, i35 = 0, i25 = 0;
    while (i5 < n && lvl (i5) > -5.0) ++i5;
    for (size_t i = i5; i < n; ++i) { if (! i25 && lvl (i) <= -25.0) i25 = i; if (lvl (i) <= -35.0) { i35 = i; break; } }
    const double drop = i35 ? 30.0 : 20.0;
    const size_t iEnd = i35 ? i35 : i25;
    if (! iEnd) return -1.0; // did not decay far enough in the window
    // least squares slope between i5 and iEnd
    double sx = 0, sy = 0, sxx = 0, sxy = 0; int m = 0;
    for (size_t i = i5; i <= iEnd; i += 16) { const double t = (double) i / SR, y = lvl (i); sx += t; sy += y; sxx += t * t; sxy += t * y; ++m; }
    const double slope = (m * sxy - sx * sy) / std::max (1e-12, m * sxx - sx * sx);
    (void) drop;
    return slope < 0 ? -60.0 / slope : -1.0;
}

double energyDb (const Buf& b) { double s = 0; for (size_t i = 0; i < b.L.size(); ++i) s += (double) b.L[i] * b.L[i] + (double) b.R[i] * b.R[i]; return 10.0 * std::log10 (std::max (1e-30, s)); }

/** Time (ms after the impulse) at which normalised echo density first reaches 0.9 (20 ms windows). */
double densityTimeMs (const std::vector<float>& x, size_t start)
{
    const int W = (int) (0.02 * SR);
    for (size_t c = start; c + (size_t) W < x.size(); c += (size_t) (W / 4))
    {
        double s2 = 0; for (int k = 0; k < W; ++k) s2 += (double) x[c + (size_t) k] * x[c + (size_t) k];
        const double sd = std::sqrt (s2 / W);
        if (sd < 1e-9) continue;
        int out = 0; for (int k = 0; k < W; ++k) if (std::abs (x[c + (size_t) k]) > sd) ++out;
        const double ned = ((double) out / W) / 0.3173;
        if (ned >= 0.9) return (double) (c - start) * 1000.0 / SR;
    }
    return -1.0;
}

/** Peakiness of the late tail spectrum in 150..5000 Hz: max / median of 1/12-oct smoothed magnitude, dB. */
double peakinessDb (const std::vector<float>& x, double t0, double t1)
{
    const size_t a = (size_t) (t0 * SR), b = std::min (x.size(), (size_t) (t1 * SR));
    if (b <= a + 1024) return 0.0;
    int n = 1; while (n * 2 <= (int) (b - a)) n *= 2;
    std::vector<float> mag; magnitudeSpectrum (x.data() + a, n, mag);
    const double binHz = SR / n;
    std::vector<double> sm;
    for (int k = (int) (150 / binHz); k < (int) (5000 / binHz) && k < (int) mag.size(); ++k)
    {
        const double f = k * binHz, lo = f * std::exp2 (-1.0 / 24), hi = f * std::exp2 (1.0 / 24);
        // narrow peak vs local band: compare bin to local mean within 1/12 octave
        double s = 0; int c = 0;
        for (int j = (int) (lo / binHz); j <= (int) (hi / binHz) && j < (int) mag.size(); ++j) { s += mag[(size_t) j] * mag[(size_t) j]; ++c; }
        sm.push_back ((double) mag[(size_t) k] * mag[(size_t) k] / std::max (1e-30, s / std::max (1, c)));
    }
    if (sm.empty()) return 0.0;
    // 99.9th percentile of bin/local-mean ratio: exponential (noise-like) spectra give ~ 8.4 dB
    std::sort (sm.begin(), sm.end());
    return 10.0 * std::log10 (sm[(size_t) (sm.size() * 0.999)]);
}

double corr (const Buf& b, double t0, double t1)
{
    const size_t a = (size_t) (t0 * SR), e = std::min (b.L.size(), (size_t) (t1 * SR));
    double sl = 0, sr = 0, slr = 0;
    for (size_t i = a; i < e; ++i) { sl += (double) b.L[i] * b.L[i]; sr += (double) b.R[i] * b.R[i]; slr += (double) b.L[i] * b.R[i]; }
    return slr / std::max (1e-30, std::sqrt (sl * sr));
}

double meanDc (const Buf& b) { double s = 0; for (float v : b.L) s += v; return s / std::max<size_t> (1, b.L.size()); }

void reverbLine (const char* name, ParamSet p, double windowSec)
{
    double cpu = 0;
    auto in = impulse (windowSec);
    auto out = run (p, in, &cpu);
    const size_t st = (size_t) (0.05 * SR);
    std::vector<float> mono (out.L.size());
    for (size_t i = 0; i < mono.size(); ++i) mono[i] = 0.5f * (out.L[i] + out.R[i]);
    const double tb = t30 (mono, st);
    const double t125 = t30 (bandFilter (mono, 125.0), st), t1k = t30 (bandFilter (mono, 1000.0), st), t4k = t30 (bandFilter (mono, 4000.0), st);
    const double late0 = std::min (windowSec * 0.5, 0.25 + 0.15 * std::max (0.3, tb)), late1 = std::min (windowSec - 0.05, late0 + 1.0);
    std::printf ("%-44s T30 %6.2f s  (125 %5.2f | 1k %5.2f | 4k %5.2f)  E %6.1f dB  dens %6.1f ms  peaky %5.1f dB  corr %+.2f  dc %+.1e  cpu %5.0fx\n",
                 name, tb, t125, t1k, t4k, energyDb (out), densityTimeMs (mono, st), peakinessDb (mono, late0, late1),
                 corr (out, late0, late1), meanDc (out), cpu);
    if (! wavDir.empty())
    {
        std::string f = name; for (auto& c : f) if (! std::isalnum ((unsigned char) c)) c = '_';
        writeWav (wavDir + "/" + f + ".wav", out);
    }
}

void delayLine (const char* name, ParamSet p, double windowSec)
{
    double cpu = 0;
    auto in = impulse (windowSec);
    auto out = run (p, in, &cpu);
    const size_t st = (size_t) (0.05 * SR);
    // first two repeat peaks (envelope maxima separated by at least 20 ms)
    std::vector<std::pair<size_t, float>> peaks;
    float runMax = 0; size_t runAt = 0; size_t lastPeak = 0;
    const size_t gap = (size_t) (0.02 * SR);
    for (size_t i = st + 32; i < out.L.size() && peaks.size() < 6; ++i)
    {
        const float v = std::max (std::abs (out.L[i]), std::abs (out.R[i]));
        if (v > runMax) { runMax = v; runAt = i; }
        if (runMax > 1e-4f && i - runAt > gap && (peaks.empty() || runAt - lastPeak > gap)) { peaks.push_back ({ runAt, runMax }); lastPeak = runAt; runMax = 0; }
    }
    const double t1 = peaks.size() > 0 ? (double) (peaks[0].first - st) * 1000.0 / SR : -1;
    const double dec = peaks.size() > 2 ? 20.0 * std::log10 (peaks[2].second / std::max (1e-9f, peaks[1].second)) : 0;
    double pk = 0; for (size_t i = 0; i < out.L.size(); ++i) pk = std::max (pk, (double) std::max (std::abs (out.L[i]), std::abs (out.R[i])));
    std::printf ("%-44s first %7.1f ms  repeat decay %6.1f dB  peak %6.3f  E %6.1f dB  corr %+.2f  cpu %5.0fx\n", name, t1, dec, pk, energyDb (out),
                 corr (out, 0.05, windowSec), cpu);
    if (! wavDir.empty())
    {
        std::string f = name; for (auto& c : f) if (! std::isalnum ((unsigned char) c)) c = '_';
        writeWav (wavDir + "/" + f + ".wav", out);
    }
}

/** Sustained loud input at maximum feedback: reports peak and final RMS (stability / runaway check). */
void stressLine (const char* name, ParamSet p, double sec)
{
    Buf in; in.L.assign ((size_t) (sec * SR), 0.0f); in.R = in.L;
    Rng r (9);
    for (size_t i = 0; i < (size_t) (2.0 * SR); ++i) { in.L[i] = r.bi() * 0.9f; in.R[i] = r.bi() * 0.9f; }
    auto out = run (p, in);
    double pk = 0; bool fin = true;
    for (size_t i = 0; i < out.L.size(); ++i) { if (! std::isfinite (out.L[i]) || ! std::isfinite (out.R[i])) fin = false; pk = std::max (pk, (double) std::abs (out.L[i])); }
    const size_t a = out.L.size() - (size_t) SR;
    std::printf ("%-44s peak %7.3f  last-second rms %8.5f  %s\n", name, pk, rms (out.L.data() + a, (int) SR), fin ? "finite" : "NON-FINITE");
}
} // namespace

int main (int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (! std::strcmp (argv[i], "--sr") && i + 1 < argc) SR = std::atof (argv[++i]);
        else if (! std::strcmp (argv[i], "--wav") && i + 1 < argc) wavDir = argv[++i];
    }
    std::printf ("pa_spacelab @ %.0f Hz\n", SR);
    const int nReverb = (int) paramChoices (ReverbMode).size(), nDelay = (int) paramChoices (DelayMode).size();
    std::printf ("reverb modes %d, delay modes %d\n\n-- reverbs (impulse) --\n", nReverb, nDelay);

    auto rv = [] (int mode) { ParamSet p = base(); p[ReverbEnable] = 1; p[ReverbMode] = (float) mode; return p; };
    { auto p = rv (0); reverbLine ("Plate default (8 s)", p, 12); }
    { auto p = rv (0); p[PlDecay] = 1.5f; reverbLine ("Plate 1.5 s", p, 5); }
    { auto p = rv (0); p[PlDecay] = 3.0f; p[PlTone] = 12000; reverbLine ("Plate 3 s bright", p, 7); }
    { auto p = rv (0); p[PlDecay] = 3.0f; p[PlSize] = 50; reverbLine ("Plate 3 s size 50", p, 7); }
    { auto p = rv (0); p[PlDecay] = 3.0f; p[PlSize] = 150; reverbLine ("Plate 3 s size 150", p, 7); }
    { auto p = rv (0); p[PlDecay] = 3.0f; p[PlDiffusion] = 20; reverbLine ("Plate 3 s diffusion 20", p, 7); }
    { auto p = rv (0); p[PlDecay] = 3.0f; p[PlMotion] = 0; reverbLine ("Plate 3 s motion 0", p, 7); }
    { auto p = rv (1); reverbLine ("Wash default (24 s)", p, 30); }
    { auto p = rv (1); p[WaDecay] = 6; reverbLine ("Wash 6 s", p, 10); }
    { auto p = rv (1); p[WaDecay] = 6; p[WaSize] = 25; reverbLine ("Wash 6 s size 25", p, 10); }
    { auto p = rv (1); p[WaDecay] = 6; p[WaBloom] = 0; reverbLine ("Wash 6 s bloom 0", p, 10); }
    { auto p = rv (1); p[WaDecay] = 6; p[WaMotion] = 0; reverbLine ("Wash 6 s motion 0", p, 10); }
    { auto p = rv (1); p[WaDecay] = 6; p[Quality] = 0; reverbLine ("Wash 6 s Eco (8 lines)", p, 10); }
    { auto p = rv (1); p[WaDecay] = 6; p[WaMotion] = 0; p[WaBloom] = 0; p[WaLowRatio] = 1; p[WaTone] = 20000; reverbLine ("Wash 6 s pure (no motion/bloom, flat)", p, 10); }
    for (int m = 2; m < nReverb; ++m)
    {
        std::string label = std::string (paramChoices (ReverbMode)[(size_t) m]);
        { auto p = rv (m); reverbLine ((label + " default").c_str(), p, 10); }
    }
    { auto p = rv (2); p[HaSize] = 8; p[HaDecay] = 0.5f; reverbLine ("Hall small room 0.5 s", p, 3); }
    { auto p = rv (2); p[HaSize] = 30; p[HaDecay] = 1.2f; reverbLine ("Hall medium room 1.2 s", p, 4); }
    { auto p = rv (2); p[HaSize] = 100; p[HaDecay] = 6; reverbLine ("Hall large 6 s", p, 12); }
    { auto p = rv (2); p[HaSize] = 100; p[HaDecay] = 6; p[HaMotion] = 0; reverbLine ("Hall large 6 s motion 0", p, 12); }
    { auto p = rv (2); p[HaDiffusion] = 0; reverbLine ("Hall default diffusion 0", p, 8); }
    { auto p = rv (2); p[HaEarly] = 0; reverbLine ("Hall default no early", p, 8); }
    { auto p = rv (2); p[HaLowRatio] = 2.0f; reverbLine ("Hall default bass x2", p, 10); }
    { auto p = rv (0); p[Shimmer] = 60; reverbLine ("Plate shimmer 60", p, 12); }
    { auto p = rv (1); p[Shimmer] = 100; reverbLine ("Wash shimmer 100", p, 30); }

    std::printf ("\n-- delays (impulse) --\n");
    auto dl = [] (int mode) { ParamSet p = base(); p[DelayEnable] = 1; p[DelayMode] = (float) mode; return p; };
    { auto p = dl (0); delayLine ("BBD default", p, 4); }
    { auto p = dl (0); p[BbdFeedback] = 95; delayLine ("BBD max feedback", p, 8); }
    { auto p = dl (0); p[BbdStereo] = 1; delayLine ("BBD ping-pong", p, 4); }
    { auto p = dl (1); delayLine ("Interval default", p, 6); }
    { auto p = dl (2); p[TpHeads] = 0; delayLine ("Tape head 1 only", p, 6); }
    { auto p = dl (2); p[TpHeads] = 0; p[TpFeedback] = 100; delayLine ("Tape head 1 fb 100", p, 10); }
    { auto p = dl (2); p[TpHeads] = 6; delayLine ("Tape heads 1+2+3", p, 6); }
    for (int m = 2; m < nDelay; ++m)
    {
        std::string label = std::string (paramChoices (DelayMode)[(size_t) m]);
        { auto p = dl (m); delayLine ((label + " default").c_str(), p, 6); }
    }

    std::printf ("\n-- stress (2 s loud noise, maximum feedback / decay) --\n");
    { auto p = dl (0); p[BbdFeedback] = 95; p[BbdAge] = 0; p[BbdTone] = 16000; stressLine ("BBD fb 95 tone 16k age 0", p, 12); }
    { auto p = dl (1); p[IvFeedback] = 90; p[IvShiftPlace] = 1; stressLine ("Interval fb 90 cascade", p, 12); }
    { auto p = rv (0); p[PlDecay] = 30; p[PlTone] = 20000; p[PlLowRatio] = 1.5f; stressLine ("Plate 30 s open", p, 12); }
    { auto p = rv (1); p[WaDecay] = 120; p[WaTone] = 20000; p[WaLowRatio] = 1.5f; stressLine ("Wash 120 s open", p, 12); }
    for (int m = 2; m < nReverb; ++m) { auto p = rv (m); stressLine ((std::string (paramChoices (ReverbMode)[(size_t) m]) + " max").c_str(), p, 12); }
    { auto p = dl (2); p[TpFeedback] = 110; p[TpHeads] = 6; p[TpDrive] = 100; stressLine ("Tape fb 110 all heads drive 100", p, 12); }
    { auto p = dl (2); p[TpFeedback] = 110; p[TpHeads] = 0; p[TpDrive] = 0; p[TpTone] = 12000; stressLine ("Tape fb 110 head 1 drive 0 bright", p, 12); }
    { auto p = rv (1); p[WaDecay] = 120; p[Shimmer] = 100; p[WaTone] = 20000; stressLine ("Wash 120 s shimmer 100", p, 12); }
    { auto p = rv (2); p[HaDecay] = 20; p[Shimmer] = 100; p[ShimmerPitch] = 1; stressLine ("Hall 20 s shimmer 100 +7", p, 12); }
    { auto p = rv (0); p[PlDecay] = 30; p[Shimmer] = 100; p[ShimmerPitch] = 4; stressLine ("Plate 30 s shimmer 100 -12", p, 12); }
    for (int sp = 0; sp < 5; ++sp)
    {
        auto p = rv (2); p[HaDecay] = 4; p[Shimmer] = 100; p[ShimmerPitch] = (float) sp;
        stressLine (("Hall 4 s shimmer 100 pitch " + std::to_string (sp) + " (40 s)").c_str(), p, 40);
    }
    { auto p = rv (0); p[PlDecay] = 8; p[Shimmer] = 100; p[ShimmerPitch] = 4; stressLine ("Plate 8 s shimmer 100 -12 (40 s)", p, 40); }
    for (int m = 2; m < nDelay; ++m) { auto p = dl (m); stressLine ((std::string (paramChoices (DelayMode)[(size_t) m]) + " max").c_str(), p, 12); }
    return 0;
}
