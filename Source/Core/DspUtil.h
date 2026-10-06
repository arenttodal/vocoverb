// Playable Ambience — shared real-time DSP helpers (platform independent, no allocation).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
  #include <xmmintrin.h>
  #define PA_HAS_SSE 1
#endif

namespace pa
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr double kPiD = 3.14159265358979323846;

inline float dbToGain (float db) noexcept { return db <= -59.9f ? 0.0f : std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return g <= 1.0e-6f ? -120.0f : 20.0f * std::log10 (g); }
inline float midiToHz (float note, float a4 = 440.0f) noexcept { return a4 * std::exp2 ((note - 69.0f) / 12.0f); }
inline float clampf (float v, float lo, float hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
inline bool isFiniteF (float v) noexcept { return std::isfinite (v); }

/** Time-constant to one-pole coefficient: y += c * (x - y). timeSec is ~63% rise time. */
inline float onePoleCoef (float timeSec, double sr) noexcept
{
    if (timeSec <= 0.0f) return 1.0f;
    return 1.0f - (float) std::exp (-1.0 / (timeSec * sr));
}

/** Coefficient so that a value decays by 60 dB in t60 seconds when multiplied each sample. */
inline float t60Coef (float t60, double sr) noexcept
{
    if (t60 <= 0.0f) return 0.0f;
    return (float) std::exp (-6.907755278982137 / (t60 * sr));
}

/** Rational tanh approximation, accurate & bounded to |y| < 1. */
inline float fastTanh (float x) noexcept
{
    x = clampf (x, -4.5f, 4.5f);
    const float x2 = x * x;
    const float y = x * (27.0f + x2) / (27.0f + 9.0f * x2);
    return clampf (y, -1.0f, 1.0f);
}

/** Flushes denormals for the lifetime of the object (x86 SSE + ARM64). */
struct ScopedFlushDenormals
{
    ScopedFlushDenormals() noexcept
    {
#if PA_HAS_SSE
        prev = _mm_getcsr();
        _mm_setcsr (prev | 0x8040); // FTZ | DAZ
#elif defined(__aarch64__)
        uint64_t fpcr;
        asm volatile ("mrs %0, fpcr" : "=r"(fpcr));
        prev = fpcr;
        asm volatile ("msr fpcr, %0" ::"r"(fpcr | (1ull << 24)));
#endif
    }
    ~ScopedFlushDenormals() noexcept
    {
#if PA_HAS_SSE
        _mm_setcsr ((unsigned int) prev);
#elif defined(__aarch64__)
        asm volatile ("msr fpcr, %0" ::"r"(prev));
#endif
    }
    uint64_t prev = 0;
};

/** Small deterministic PRNG (xorshift32). */
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    explicit Rng (uint32_t seed = 0x9E3779B9u) : s (seed ? seed : 1u) {}
    inline uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); } // [0,1)
    inline float bi() noexcept { return uni() * 2.0f - 1.0f; }                             // [-1,1)
};

/** Linear ramp smoother advanced per sample or per chunk. */
struct Smoothed
{
    float cur = 0.0f, target = 0.0f, step = 0.0f;
    int remaining = 0, rampLen = 1;
    void setRampLength (int samples) noexcept { rampLen = std::max (1, samples); }
    void reset (float v) noexcept { cur = target = v; remaining = 0; step = 0; }
    void set (float v) noexcept
    {
        if (v == target) return;
        target = v; remaining = rampLen; step = (target - cur) / (float) rampLen;
    }
    inline float next() noexcept
    {
        if (remaining > 0) { cur += step; if (--remaining == 0) cur = target; }
        return cur;
    }
    void skip (int n) noexcept
    {
        if (remaining <= 0) return;
        if (n >= remaining) { cur = target; remaining = 0; }
        else { cur += step * (float) n; remaining -= n; }
    }
    bool isSmoothing() const noexcept { return remaining > 0; }
};

/** RBJ biquad, transposed direct form II. */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;
    void reset() noexcept { z1 = z2 = 0; }
    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setNormalized (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
    void lowpass (double sr, double f, double q = 0.7071) noexcept
    {
        f = std::clamp (f, 10.0, sr * 0.49);
        const double w = 2.0 * kPiD * f / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
        setNormalized ((1 - c) * 0.5, 1 - c, (1 - c) * 0.5, 1 + a, -2 * c, 1 - a);
    }
    void highpass (double sr, double f, double q = 0.7071) noexcept
    {
        f = std::clamp (f, 5.0, sr * 0.49);
        const double w = 2.0 * kPiD * f / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
        setNormalized ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + a, -2 * c, 1 - a);
    }
    /** Constant 0 dB peak gain band-pass. */
    void bandpass (double sr, double f, double q) noexcept
    {
        f = std::clamp (f, 10.0, sr * 0.49);
        const double w = 2.0 * kPiD * f / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
        setNormalized (a, 0, -a, 1 + a, -2 * c, 1 - a);
    }
    void lowShelf (double sr, double f, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPiD * std::clamp (f, 10.0, sr * 0.45) / sr;
        const double c = std::cos (w), s = std::sin (w), al = s / 2.0 * std::sqrt (2.0);
        const double sq = 2.0 * std::sqrt (A) * al;
        setNormalized (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
                       (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (double sr, double f, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPiD * std::clamp (f, 10.0, sr * 0.45) / sr;
        const double c = std::cos (w), s = std::sin (w), al = s / 2.0 * std::sqrt (2.0);
        const double sq = 2.0 * std::sqrt (A) * al;
        setNormalized (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
                       (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

/** One-pole low-pass (exact matched coefficient). */
struct OnePoleLP
{
    float a = 1.0f, z = 0.0f;
    void setCutoff (double sr, double f) noexcept { a = 1.0f - (float) std::exp (-2.0 * kPiD * std::clamp (f, 1.0, sr * 0.49) / sr); }
    void setOpen() noexcept { a = 1.0f; }
    /** Decay-consistent damping for a feedback segment of `segSamples`: unity at DC, and at `toneHz` an extra loss of
        `extra` times the segment's broadband loss for `t60` seconds. The decay at toneHz is then t60 / (1 + extra) for
        every segment length (Jot-style absorbent filter), so Tone no longer depends on the line lengths, Size or
        Decay. Opens fully when the required loss is negligible. */
    void setDecayDamping (double sr, double toneHz, double segSamples, double t60, double extra) noexcept
    {
        const double w = 2.0 * kPiD * std::clamp (toneHz, 20.0, sr * 0.45) / sr;
        const double T = std::pow (10.0, -3.0 * extra * segSamples / (sr * std::max (0.05, t60)));
        if (T > 0.99999) { a = 1.0f; return; }
        // |a / (1 - r e^-jw)| = T with a = 1 - r  ->  (T^2 - 1) r^2 + (2 - 2 T^2 cos w) r + (T^2 - 1) = 0
        const double A = T * T - 1.0, B = 2.0 - 2.0 * T * T * std::cos (w);
        const double disc = B * B - 4.0 * A * A;
        if (disc < 0.0) { a = 1.0f; return; }
        double r = (-B + std::sqrt (disc)) / (2.0 * A);
        if (r < 0.0 || r >= 1.0) r = (-B - std::sqrt (disc)) / (2.0 * A);
        r = std::clamp (r, 0.0, 0.9995);
        a = (float) (1.0 - r);
    }
    inline float process (float x) noexcept { z += a * (x - z); return z; }
    void reset() noexcept { z = 0; }
};

/** DC blocker / gentle high-pass. */
struct DcBlocker
{
    float r = 0.995f, x1 = 0, y1 = 0;
    void setCutoff (double sr, double f) noexcept { r = (float) std::exp (-2.0 * kPiD * f / sr); }
    inline float process (float x) noexcept { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() noexcept { x1 = y1 = 0; }
};

/** Preallocated circular delay line with fractional reads and progressive clearing. */
struct DelayLine
{
    std::vector<float> buf;
    int size = 0, mask = 0, w = 0;
    int clearPos = -1; // >=0 while a progressive clear is in progress

    void allocate (int minSamples)
    {
        int s = 1;
        while (s < minSamples + 4) s <<= 1;
        buf.assign ((size_t) s, 0.0f);
        size = s; mask = s - 1; w = 0; clearPos = -1;
    }
    void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); clearPos = -1; }
    void beginProgressiveClear() noexcept { clearPos = 0; }
    /** Returns true when done. */
    bool clearStep (int budget) noexcept
    {
        if (clearPos < 0) return true;
        const int n = std::min (budget, size - clearPos);
        std::memset (buf.data() + clearPos, 0, sizeof (float) * (size_t) n);
        clearPos += n;
        if (clearPos >= size) { clearPos = -1; return true; }
        return false;
    }
    inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
    /** Read integer delay d >= 1 (1 = most recently pushed sample). */
    inline float readInt (int d) const noexcept { return buf[(size_t) ((w - d) & mask)]; }
    inline float readLinear (float d) const noexcept
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = buf[(size_t) ((w - i) & mask)], b = buf[(size_t) ((w - i - 1) & mask)];
        return a + (b - a) * f;
    }
    /** Cubic Hermite read; d must be >= 2. */
    inline float readCubic (float d) const noexcept
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float xm1 = buf[(size_t) ((w - i + 1) & mask)];
        const float x0 = buf[(size_t) ((w - i) & mask)];
        const float x1 = buf[(size_t) ((w - i - 1) & mask)];
        const float x2 = buf[(size_t) ((w - i - 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    int maxDelay() const noexcept { return size - 4; }
};

/** Schroeder all-pass with an internal delay line (integer or modulated fractional delay). */
struct AllpassDelay
{
    DelayLine line;
    float g = 0.5f;
    void allocate (int maxSamples) { line.allocate (maxSamples + 8); }
    inline float process (float x, float delay) noexcept
    {
        const float d = delay >= 2.0f ? line.readCubic (delay) : line.readLinear (std::max (1.0f, delay));
        const float v = x + g * d;
        line.push (v);
        return d - g * v;
    }
    void clear() noexcept { line.clear(); }
};

/** Polyphase-free sine LFO via phase accumulator. */
struct Lfo
{
    double phase = 0.0, inc = 0.0;
    void setRate (double hz, double sr) noexcept { inc = hz / sr; }
    inline float next() noexcept
    {
        const float v = std::sin ((float) (phase * 2.0 * kPiD));
        phase += inc; if (phase >= 1.0) phase -= 1.0;
        return v;
    }
    void advance (int n) noexcept { phase += inc * n; phase -= std::floor (phase); }
    float value() const noexcept { return std::sin ((float) (phase * 2.0 * kPiD)); }
};

/** Smoothed random drift in [-1,1], updated per sample with low-pass interpolation. */
struct Drift
{
    Rng rng { 12345u };
    float cur = 0, target = 0, coef = 0.001f;
    int counter = 0, period = 4800;
    void set (double rateHz, double sr) noexcept
    {
        period = std::max (16, (int) (sr / std::max (0.01, rateHz)));
        coef = onePoleCoef ((float) (0.5 / std::max (0.01, rateHz)), sr);
    }
    inline float next() noexcept
    {
        if (--counter <= 0) { counter = period; target = rng.bi(); }
        cur += coef * (target - cur);
        return cur;
    }
};

/** Stereo sample pair helper. */
struct Stereo { float l = 0, r = 0; };

/** Constant-power pan gains for p in [-1, 1]. */
inline void panGains (float p, float& gl, float& gr) noexcept
{
    const float a = (clampf (p, -1.0f, 1.0f) + 1.0f) * (kPi * 0.25f);
    gl = std::cos (a) * 1.41421356f;
    gr = std::sin (a) * 1.41421356f;
}

/** Returns false if any sample is not finite. */
inline bool allFinite (const float* x, int n) noexcept
{
    float acc = 0.0f;
    for (int i = 0; i < n; ++i) acc += x[i] * 0.0f;
    return acc == 0.0f; // NaN/Inf * 0 = NaN
}

} // namespace pa
