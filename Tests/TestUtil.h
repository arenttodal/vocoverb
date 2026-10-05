#pragma once

#include "Core/Analysis.h"
#include "Core/Engine.h"
#include "Core/Fixtures.h"
#include "Core/Sequences.h"
#include "Presets/FactoryPresets.h"
#include "TestFramework.h"

#include <functional>
#include <memory>
#include <vector>

namespace pat
{
struct Buf { std::vector<float> L, R; size_t size() const { return L.size(); } };

inline Buf silence (double sec, double sr = 48000.0) { Buf s; s.L.assign ((size_t) (sec * sr), 0.0f); s.R = s.L; return s; }

inline Buf noiseBurst (double burstSec, double totalSec, double sr = 48000.0, float amp = 0.25f, uint32_t seed = 1)
{
    Buf s = silence (totalSec, sr);
    pa::Rng r (seed);
    for (size_t i = 0; i < (size_t) (burstSec * sr) && i < s.size(); ++i) { s.L[i] = r.bi() * amp; s.R[i] = r.bi() * amp; }
    return s;
}

inline Buf sine (double hz, double sec, double sr = 48000.0, float amp = 0.3f)
{
    Buf s = silence (sec, sr);
    for (size_t i = 0; i < s.size(); ++i) s.L[i] = s.R[i] = amp * (float) std::sin (2.0 * 3.14159265358979 * hz * (double) i / sr);
    return s;
}

using Automation = std::function<void (double timeSec, pa::ParamSet&)>;

/** Renders through a fresh engine. Events are timed in seconds. */
inline Buf render (const pa::ParamSet& params, const Buf& in, const std::vector<pa::TimedEvent>& events = {},
                      double sr = 48000.0, int block = 256, Automation automation = nullptr, pa::Engine* engineOut = nullptr,
                      pa::Origin origin = pa::Origin::Host)
{
    auto eng = std::make_unique<pa::Engine>();
    pa::ParamSet p = params;
    // engine reset reads its own copy of params for initial states: process one silent block first
    eng->prepare (sr, block);
    Buf out; out.L.assign (in.size(), 0.0f); out.R.assign (in.size(), 0.0f);
    std::vector<pa::MidiEvent> ev (512);
    pa::TransportInfo tp;
    for (size_t pos = 0; pos < in.size(); pos += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, in.size() - pos);
        const double t0 = (double) pos / sr, t1 = (double) (pos + (size_t) n) / sr;
        if (automation) automation (t0, p);
        const int ne = pa::eventsInWindow (events, t0, t1, sr, ev.data(), (int) ev.size(), origin);
        eng->process (p, tp, in.L.data() + pos, in.R.data() + pos, out.L.data() + pos, out.R.data() + pos, n, ev.data(), ne);
    }
    if (engineOut) { /* caller wants state: not supported for unique engine */ }
    return out;
}

inline std::vector<pa::TimedEvent> chordEvents (double t0, double t1, std::initializer_list<int> notes, int vel = 100)
{
    std::vector<pa::TimedEvent> e;
    for (int n : notes) e.push_back ({ t0, 0x90, (uint8_t) n, (uint8_t) vel });
    if (t1 > t0) for (int n : notes) e.push_back ({ t1, 0x80, (uint8_t) n, 0 });
    return e;
}

inline void append (std::vector<pa::TimedEvent>& a, const std::vector<pa::TimedEvent>& b)
{
    a.insert (a.end(), b.begin(), b.end());
    std::stable_sort (a.begin(), a.end(), [] (const pa::TimedEvent& x, const pa::TimedEvent& y) { return x.time < y.time; });
}

inline double segRms (const std::vector<float>& x, double sr, double t0, double t1)
{
    const size_t a = (size_t) (t0 * sr), b = std::min (x.size(), (size_t) (t1 * sr));
    return b > a ? pa::rms (x.data() + a, (int) (b - a)) : 0.0;
}
inline double segPeak (const std::vector<float>& x, double sr, double t0, double t1)
{
    const size_t a = (size_t) (t0 * sr), b = std::min (x.size(), (size_t) (t1 * sr));
    return b > a ? pa::peakAbs (x.data() + a, (int) (b - a)) : 0.0;
}
inline double db (double v) { return v <= 1e-12 ? -240.0 : 20.0 * std::log10 (v); }
inline bool finite (const Buf& s) { for (size_t i = 0; i < s.size(); ++i) if (! std::isfinite (s.L[i]) || ! std::isfinite (s.R[i])) return false; return true; }

/** Params with only the reverb (plate) enabled, Live timing, no duck, given method. */
inline pa::ParamSet baseParams (int method = 1)
{
    pa::ParamSet p;
    p[pa::DelayEnable] = 0; p[pa::ReverbEnable] = 1; p[pa::ReverbMode] = 0;
    p[pa::HarmMethod] = (float) method; p[pa::Depth] = 100; p[pa::DuckAmount] = 0;
    p[pa::Timing] = 0; p[pa::WetLevel] = 0; p[pa::DryLevel] = -60; p[pa::WetOnly] = 1;
    p[pa::WetLowCut] = 20; p[pa::WetHighCut] = 20000;
    return p;
}

/** Fraction of the segment's energy lying on the harmonics of the given notes (within cents). */
inline double chordFraction (const std::vector<float>& x, double sr, double t0, double t1, std::initializer_list<int> notes, double cents = 15.0)
{
    const size_t a = (size_t) (t0 * sr), b = std::min (x.size(), (size_t) (t1 * sr));
    const int n = (int) (b - a);
    std::vector<float> mag;
    pa::magnitudeSpectrum (x.data() + a, n, mag);
    const int N = (int) (mag.size() - 1) * 2;
    const double binHz = sr / N;
    std::vector<char> mark (mag.size(), 0);
    for (int nn : notes)
    {
        const double f0 = 440.0 * std::exp2 ((nn - 69) / 12.0);
        for (int h = 1; h <= 40; ++h)
        {
            const double f = f0 * h;
            if (f > 2500.0) break;
            const double lo = f * std::exp2 (-cents / 1200.0), hi = f * std::exp2 (cents / 1200.0);
            for (int k = std::max (0, (int) std::floor (lo / binHz)); k <= std::min ((int) mag.size() - 1, (int) std::ceil (hi / binHz)); ++k) mark[(size_t) k] = 1;
        }
    }
    double tot = 0, in = 0;
    for (size_t k = (size_t) (60.0 / binHz); k < mag.size() && k * binHz < 2500.0; ++k)
    {
        const double e = (double) mag[k] * mag[k];
        tot += e; if (mark[k]) in += e;
    }
    return tot > 0 ? in / tot : 0.0;
}
} // namespace pat
