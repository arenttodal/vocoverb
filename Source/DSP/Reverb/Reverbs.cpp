#include "Reverbs.h"

namespace pa
{
namespace
{
// Dattorro reference values at 29761 Hz.
constexpr double kRefSr = 29761.0;
constexpr float kDiffLen[4] = { 142, 107, 379, 277 };
constexpr float kApModA = 672, kD1A = 4453, kAp2A = 1800, kD2A = 3720;
constexpr float kApModB = 908, kD1B = 4217, kAp2B = 2656, kD2B = 3163;
constexpr float kTankTotal = kApModA + kD1A + kAp2A + kD2A + kApModB + kD1B + kAp2B + kD2B;

inline void hadamard16 (float* x, int n) noexcept
{
    for (int len = 1; len < n; len <<= 1)
        for (int i = 0; i < n; i += len << 1)
            for (int j = i; j < i + len; ++j)
            {
                const float a = x[j], b = x[j + len];
                x[j] = a + b;
                x[j + len] = a - b;
            }
    const float s = 1.0f / std::sqrt ((float) n);
    for (int i = 0; i < n; ++i) x[i] *= s;
}
} // namespace

// ============================================================================ Plate
void PlateReverb::prepare (double sampleRate)
{
    sr = sampleRate;
    scale = sr / kRefSr;
    const double maxSize = 1.55;
    pre.allocate ((int) (0.26 * sr) + 8);
    preR.allocate ((int) (0.26 * sr) + 8);
    for (int k = 0; k < 4; ++k)
    {
        diffL[k].allocate ((int) (kDiffLen[k] * scale * 1.1) + 8);
        diffR[k].allocate ((int) (kDiffLen[k] * scale * 1.1) + 8);
    }
    const int exc = (int) (40 * scale) + 8;
    A.apMod.allocate ((int) (kApModA * scale * maxSize) + exc);
    A.d1.allocate ((int) (kD1A * scale * maxSize) + 8);
    A.ap2.allocate ((int) (kAp2A * scale * maxSize) + 8);
    A.d2.allocate ((int) (kD2A * scale * maxSize) + 8);
    B.apMod.allocate ((int) (kApModB * scale * maxSize) + exc);
    B.d1.allocate ((int) (kD1B * scale * maxSize) + 8);
    B.ap2.allocate ((int) (kAp2B * scale * maxSize) + 8);
    B.d2.allocate ((int) (kD2B * scale * maxSize) + 8);
    clears.count = 0;
    clears.add (pre);
    clears.add (preR);
    for (int k = 0; k < 4; ++k) { clears.add (diffL[k].line); clears.add (diffR[k].line); }
    for (Half* h : { &A, &B }) { clears.add (h->apMod.line); clears.add (h->d1); clears.add (h->ap2.line); clears.add (h->d2); }
    guard.prepare (sr);
    drift.set (0.07, sr);
    reset();
}

void PlateReverb::resetSmallState() noexcept
{
    bandL.reset(); bandR.reset();
    for (Half* h : { &A, &B }) { h->damp.reset(); h->lowSplit1.reset(); h->lowSplit2.reset(); }
    fbA = fbB = 0.0f;
    guard.ms = 0.0f; guard.ref = 0.0f;
}

void PlateReverb::reset() noexcept
{
    pre.clear(); preR.clear();
    for (int k = 0; k < 4; ++k) { diffL[k].clear(); diffR[k].clear(); }
    for (Half* h : { &A, &B }) { h->apMod.clear(); h->d1.clear(); h->ap2.clear(); h->d2.clear(); }
    resetSmallState();
    lfoA.phase = 0.0; lfoB.phase = 0.25;
}

void PlateReverb::process (const SpaceContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    const ParamSet& p = *ctx.p;
    const bool frozen = ctx.freeze;
    const float diff = p[PlDiffusion] / 100.0f;
    const float sizeT = p[PlSize] / 100.0f;
    const float motion = clampf (p[PlMotion] / 100.0f + ctx.motionMod, 0.0f, 1.0f);
    const float t60 = frozen ? 1.0e4f : std::max (0.3f, p[PlDecay]);
    const float lowRatio = frozen ? 1.0f : p[PlLowRatio];
    const float preS = std::max (1.0f, p[PlPredelay] * 0.001f * (float) sr);
    lfoA.setRate (p[PlRate], sr);
    lfoB.setRate (p[PlRate] * 0.83, sr);
    const float inBand = std::min (18000.0f, p[PlTone] * 1.6f);
    bandL.setCutoff (sr, inBand); bandR.setCutoff (sr, inBand);
    for (Half* h : { &A, &B })
    {
        if (frozen) h->damp.setOpen(); else h->damp.setCutoff (sr, p[PlTone]);
        h->lowSplit1.setCutoff (sr, 250.0);
        h->lowSplit2.setCutoff (sr, 250.0);
    }
    for (int k = 0; k < 4; ++k)
    {
        const float g = (k < 2 ? 0.75f : 0.625f) * (0.25f + 0.75f * diff);
        diffL[k].g = g; diffR[k].g = g;
    }
    A.apMod.g = -0.7f * (0.3f + 0.7f * diff); B.apMod.g = A.apMod.g;
    A.ap2.g = 0.5f; B.ap2.g = 0.5f;
    const float sizeCoef = onePoleCoef (0.4f, sr);
    const float exc = motion * 16.0f * (float) scale * 1.5f;
    const float sc = (float) scale;
    const float overdub = ctx.overdub;

    const float loopSec = kTankTotal * sizeSm * sc / (float) sr;
    float g = std::pow (10.0f, -3.0f * loopSec / (4.0f * t60));
    float gLow = std::min (0.99995f, std::pow (10.0f, -3.0f * loopSec / (4.0f * t60 * lowRatio)));
    if (frozen) { g = std::min (g, 0.99999f); gLow = g; }
    for (int i = 0; i < n; ++i)
    {
        sizeSm += sizeCoef * (sizeT - sizeSm);
        const float S = sizeSm * sc;

        const float inGain = guard.nextInputGain (frozen, overdub);
        pre.push (inL[i] * inGain);
        preR.push (inR[i] * inGain);
        float xl = pre.readLinear (preS);
        float xr = preR.readLinear (preS);
        xl = bandL.process (xl);
        xr = bandR.process (xr);
        for (int k = 0; k < 4; ++k)
        {
            xl = diffL[k].process (xl, kDiffLen[k] * sc);
            xr = diffR[k].process (xr, kDiffLen[k] * sc * 1.07f);
        }
        const float dr = drift.next();
        const float modA = exc * (lfoA.next() + 0.3f * dr), modB = exc * (lfoB.next() - 0.3f * dr);

        // half A
        float a = A.apMod.process (xl + fbB, kApModA * S + exc + modA);
        A.d1.push (a);
        float a1 = A.d1.readCubic (kD1A * S);
        a1 = A.damp.process (a1);
        { const float lo = A.lowSplit1.process (a1); a1 = g * (a1 - lo) + gLow * lo; }
        a1 = A.ap2.process (a1, kAp2A * S);
        A.d2.push (a1);
        float a2 = A.d2.readCubic (kD2A * S);
        { const float lo = A.lowSplit2.process (a2); a2 = g * (a2 - lo) + gLow * lo; }

        // half B
        float b = B.apMod.process (xr + fbA, kApModB * S + exc + modB);
        B.d1.push (b);
        float b1 = B.d1.readCubic (kD1B * S);
        b1 = B.damp.process (b1);
        { const float lo = B.lowSplit1.process (b1); b1 = g * (b1 - lo) + gLow * lo; }
        b1 = B.ap2.process (b1, kAp2B * S);
        B.d2.push (b1);
        float b2 = B.d2.readCubic (kD2B * S);
        { const float lo = B.lowSplit2.process (b2); b2 = g * (b2 - lo) + gLow * lo; }

        const float energy = a2 * a2 + b2 * b2 + a1 * a1 + b1 * b1;
        const float guardGain = guard.update (frozen, energy, (int) (0.4 * sr));
        fbA = a2 * guardGain;
        fbB = b2 * guardGain;
        if (guardGain < 1.0f)
        {
            // also bleed energy from stored states so the guard acts quickly
            fbA *= 0.999f; fbB *= 0.999f;
        }

        auto tap = [S] (const DelayLine& d, float t) { return d.readLinear (std::max (1.0f, t * S)); };
        const float yl = tap (B.d1, 266) + tap (B.d1, 2974) - tap (B.ap2.line, 1913) + tap (B.d2, 1996)
                       - tap (A.d1, 1990) - tap (A.ap2.line, 187) - tap (A.d2, 1066);
        const float yr = tap (A.d1, 353) + tap (A.d1, 3627) - tap (A.ap2.line, 1228) + tap (A.d2, 2673)
                       - tap (B.d1, 2111) - tap (B.ap2.line, 335) - tap (B.d2, 121);
        outL[i] = yl * 0.6f;
        outR[i] = yr * 0.6f;
    }
}

// ============================================================================ Wash
void WashReverb::prepare (double sampleRate)
{
    sr = sampleRate;
    static const float kLenMs[kMaxLines] = { 29.7f, 37.1f, 41.9f, 47.3f, 53.9f, 59.3f, 67.1f, 73.7f,
                                             83.3f, 89.9f, 97.3f, 107.1f, 113.9f, 127.3f, 139.1f, 151.7f };
    clears.count = 0;
    for (int i = 0; i < kMaxLines; ++i)
    {
        baseLen[i] = kLenMs[i] * 0.001f * (float) sr;
        line[i].allocate ((int) (baseLen[i] * 2.05f + 0.015 * sr) + 16);
        clears.add (line[i]);
        lfo[i].phase = std::fmod (0.137 * i * 7.0, 1.0);
    }
    static const float kDiffMs[4] = { 4.1f, 5.9f, 8.3f, 12.7f };
    static const float kBloomMs[4] = { 37.0f, 59.0f, 83.0f, 113.0f };
    for (int k = 0; k < 4; ++k)
    {
        diffL[k].allocate ((int) (kDiffMs[k] * 0.0011 * sr) + 8);
        diffR[k].allocate ((int) (kDiffMs[k] * 0.0011 * sr) + 8);
        bloomL[k].allocate ((int) (kBloomMs[k] * 0.0015 * sr) + 8);
        bloomR[k].allocate ((int) (kBloomMs[k] * 0.0015 * sr) + 8);
        clears.add (diffL[k].line); clears.add (diffR[k].line);
        clears.add (bloomL[k].line); clears.add (bloomR[k].line);
    }
    guard.prepare (sr);
    reset();
}

void WashReverb::resetSmallState() noexcept
{
    for (int i = 0; i < kMaxLines; ++i) { damp[i].reset(); lowSplit[i].reset(); y[i] = 0.0f; }
    guard.ms = 0.0f; guard.ref = 0.0f;
}

void WashReverb::reset() noexcept
{
    for (auto& l : line) l.clear();
    for (int k = 0; k < 4; ++k) { diffL[k].clear(); diffR[k].clear(); bloomL[k].clear(); bloomR[k].clear(); }
    resetSmallState();
}

void WashReverb::process (const SpaceContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    const ParamSet& p = *ctx.p;
    const int wantLines = ctx.quality == 0 ? 8 : 16;
    if (wantLines != lines) { lines = wantLines; resetSmallState(); }
    const int N = lines;
    const int stride = kMaxLines / N;
    const bool frozen = ctx.freeze;
    const float t60 = frozen ? 1.0e4f : std::max (1.0f, p[WaDecay]);
    const float lowRatio = frozen ? 1.0f : p[WaLowRatio];
    const float bloom = p[WaBloom] / 100.0f;
    const float sizeT = p[WaSize] / 100.0f;
    const float motion = clampf (p[WaMotion] / 100.0f + ctx.motionMod, 0.0f, 1.0f);
    const float depth = motion * 0.006f * (float) sr;
    for (int i = 0; i < N; ++i)
    {
        const int li = i * stride;
        lfo[li].setRate (p[WaRate] * (0.62 + 0.76 * (double) i / N), sr);
        if (frozen) damp[li].setOpen(); else damp[li].setCutoff (sr, p[WaTone]);
        lowSplit[li].setCutoff (sr, 220.0);
    }
    static const float kDiffMs[4] = { 4.1f, 5.9f, 8.3f, 12.7f };
    static const float kBloomMs[4] = { 37.0f, 59.0f, 83.0f, 113.0f };
    for (int k = 0; k < 4; ++k)
    {
        diffL[k].g = 0.62f; diffR[k].g = 0.62f;
        bloomL[k].g = 0.12f + 0.62f * bloom; bloomR[k].g = bloomL[k].g;
    }
    const float sizeCoef = onePoleCoef (0.6f, sr);
    const float srf = (float) sr;
    float gain[kMaxLines], gainLow[kMaxLines], inj[kMaxLines];
    for (int i = 0; i < N; ++i)
    {
        const float pos = (float) i / (float) (N - 1);
        inj[i] = (1.0f - bloom * 0.85f * (1.0f - pos)) * (i & 2 ? -1.0f : 1.0f);
    }
    const float outScale = 1.4f / std::sqrt ((float) N);
    for (int i = 0; i < N; ++i)
    {
        const float lenSec = baseLen[i * stride] * sizeSm / srf;
        gain[i] = std::pow (10.0f, -3.0f * lenSec / t60);
        gainLow[i] = std::min (0.99995f, std::pow (10.0f, -3.0f * lenSec / (t60 * lowRatio)));
        if (frozen) { gain[i] = std::min (gain[i], 0.99999f); gainLow[i] = gain[i]; }
    }

    for (int s = 0; s < n; ++s)
    {
        sizeSm += sizeCoef * (sizeT - sizeSm);
        const float inGain = guard.nextInputGain (frozen, ctx.overdub);
        float xl = inL[s] * inGain, xr = inR[s] * inGain;
        for (int k = 0; k < 4; ++k)
        {
            xl = diffL[k].process (xl, kDiffMs[k] * 0.001f * srf);
            xr = diffR[k].process (xr, kDiffMs[k] * 0.00107f * srf);
        }
        const float bScale = std::sqrt (sizeSm);
        for (int k = 0; k < 4; ++k)
        {
            xl = bloomL[k].process (xl, kBloomMs[k] * 0.001f * srf * bScale);
            xr = bloomR[k].process (xr, kBloomMs[k] * 0.00113f * srf * bScale);
        }
        float energy = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const int li = i * stride;
            const float len = baseLen[li] * sizeSm;
            const float d = std::max (4.0f, len + depth * (1.0f + lfo[li].next()));
            float v = line[li].readCubic (d);
            v = damp[li].process (v);
            const float lo = lowSplit[li].process (v);
            v = gain[i] * (v - lo) + gainLow[i] * lo;
            y[i] = v;
            energy += v * v;
        }
        float l = 0.0f, r = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            l += (i & 1 ? -y[i] : y[i]);
            r += ((i >> 1) & 1 ? -y[i] : y[i]) * ((i & 1) ? 1.0f : -1.0f);
        }
        const float gg = guard.update (frozen, energy, (int) (0.5 * sr));
        float fbv[kMaxLines];
        for (int i = 0; i < N; ++i) fbv[i] = y[i] * gg;
        hadamard16 (fbv, N);
        for (int i = 0; i < N; ++i)
        {
            const float in = (i & 1 ? xr : xl) * inj[i];
            line[i * stride].push (fbv[i] + in * 0.5f);
        }
        outL[s] = l * outScale;
        outR[s] = r * outScale;
    }
}
} // namespace pa
