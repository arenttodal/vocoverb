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
    {
        // decay-consistent damping: the plate's decay at Tone is half the set decay, whatever the size
        const double S0 = sizeSm * scale;
        const double segA = (kApModA + kD1A + kAp2A + kD2A) * S0, segB = (kApModB + kD1B + kAp2B + kD2B) * S0;
        if (frozen) { A.damp.setOpen(); B.damp.setOpen(); }
        else { A.damp.setDecayDamping (sr, p[PlTone], segA, t60, 1.0); B.damp.setDecayDamping (sr, p[PlTone], segB, t60, 1.0); }
    }
    for (Half* h : { &A, &B })
    {
        h->lowSplit1.setCutoff (sr, 250.0);
        h->lowSplit2.setCutoff (sr, 250.0);
    }
    for (int k = 0; k < 4; ++k)
    {
        // 85 % (default) reaches the published input-diffusion values; 100 % is slightly denser
        const float g = std::min (0.8f, (k < 2 ? 0.75f : 0.625f) * (0.25f + 0.9f * diff));
        diffL[k].g = g; diffR[k].g = g;
    }
    A.apMod.g = -std::min (0.75f, 0.7f * (0.3f + 0.85f * diff)); B.apMod.g = A.apMod.g;
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
        // decay-consistent damping: decay at Tone = decay / 2.5 on every line, independent of size and decay
        if (frozen) damp[li].setOpen(); else damp[li].setDecayDamping (sr, p[WaTone], baseLen[li] * sizeSm, t60, 1.5);
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

// ============================================================================ Hall
namespace pa
{
namespace
{
// Section lengths in ms at size 100 % (largest hall); all mutually prime-ish so the ring does not ring.
constexpr float kHaAp1[4] = { 21.7f, 27.1f, 18.9f, 29.3f };
constexpr float kHaD1[4] = { 91.3f, 79.7f, 101.9f, 85.1f };
constexpr float kHaAp2[4] = { 33.7f, 41.3f, 37.1f, 47.9f };
constexpr float kHaD2[4] = { 67.3f, 73.1f, 61.9f, 57.7f };
constexpr float kHaDiff[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
// early reflections (ms at size 100 %), gains alternate in sign and fall off with time
constexpr float kErL[12] = { 4.3f, 9.7f, 13.1f, 19.9f, 24.7f, 31.3f, 37.9f, 46.1f, 53.3f, 61.7f, 70.9f, 79.3f };
constexpr float kErR[12] = { 5.9f, 8.3f, 15.7f, 18.1f, 27.3f, 29.9f, 40.1f, 44.3f, 56.7f, 59.1f, 73.3f, 83.9f };
constexpr float kErG[12] = { 0.84f, -0.72f, 0.66f, 0.58f, -0.52f, 0.47f, -0.41f, 0.36f, 0.31f, -0.27f, 0.23f, -0.19f };
inline float hallScale (float sizeNorm) noexcept { return 0.1f + 0.9f * sizeNorm; } // 5 % ~ small room, 100 % ~ large hall
} // namespace

void HallReverb::prepare (double sampleRate)
{
    sr = sampleRate;
    const double ms = 0.001 * sr;
    pre.allocate ((int) (0.26 * sr) + 8);
    preR.allocate ((int) (0.26 * sr) + 8);
    erL.allocate ((int) (90.0 * ms) + 16);
    erR.allocate ((int) (90.0 * ms) + 16);
    erApL.allocate ((int) (4.0 * ms) + 8);
    erApR.allocate ((int) (4.0 * ms) + 8);
    for (int k = 0; k < 4; ++k) { diffL[k].allocate ((int) (kHaDiff[k] * 1.1 * ms) + 8); diffR[k].allocate ((int) (kHaDiff[k] * 1.2 * ms) + 8); }
    const int modExtra = (int) (2.5 * ms) + 8;
    clears.count = 0;
    clears.add (pre); clears.add (preR); clears.add (erL); clears.add (erR); clears.add (erApL.line); clears.add (erApR.line);
    for (int k = 0; k < 4; ++k) { clears.add (diffL[k].line); clears.add (diffR[k].line); }
    for (int j = 0; j < kSections; ++j)
    {
        auto& s = sec[j];
        s.ap1.allocate ((int) (kHaAp1[j] * ms) + modExtra);
        s.d1.allocate ((int) (kHaD1[j] * ms) + 8);
        s.ap2.allocate ((int) (kHaAp2[j] * ms) + 8);
        s.d2.allocate ((int) (kHaD2[j] * ms) + 8);
        s.wander.rng = Rng (1013u + 7919u * (uint32_t) j);
        clears.add (s.ap1.line); clears.add (s.d1); clears.add (s.ap2.line); clears.add (s.d2);
    }
    hpL.setCutoff (sr, 25.0); hpR.setCutoff (sr, 25.0);
    guard.prepare (sr);
    reset();
}

void HallReverb::resetSmallState() noexcept
{
    bandL.reset(); bandR.reset(); hpL.reset(); hpR.reset();
    for (auto& s : sec) { s.damp.reset(); s.low.reset(); s.out = 0.0f; }
    guard.ms = 0.0f; guard.ref = 0.0f;
}

void HallReverb::reset() noexcept
{
    pre.clear(); preR.clear(); erL.clear(); erR.clear(); erApL.clear(); erApR.clear();
    for (int k = 0; k < 4; ++k) { diffL[k].clear(); diffR[k].clear(); }
    for (int j = 0; j < kSections; ++j) { auto& s = sec[j]; s.ap1.clear(); s.d1.clear(); s.ap2.clear(); s.d2.clear(); s.spin.phase = 0.25 * j; }
    resetSmallState();
}

void HallReverb::process (const SpaceContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    const ParamSet& p = *ctx.p;
    const bool frozen = ctx.freeze;
    const float t60 = frozen ? 1.0e4f : std::max (0.2f, p[HaDecay]);
    const float lowRatio = frozen ? 1.0f : p[HaLowRatio];
    const float sizeT = hallScale (clampf ((p[HaSize] - 5.0f) / 95.0f, 0.0f, 1.0f));
    const float diff = p[HaDiffusion] / 100.0f;
    const float early = p[HaEarly] / 100.0f;
    const float motion = clampf (p[HaMotion] / 100.0f + ctx.motionMod, 0.0f, 1.0f);
    const float ms = 0.001f * (float) sr;
    const float preS = std::max (1.0f, p[HaPredelay] * ms);
    const float inBand = std::min (18000.0f, p[HaTone] * 1.5f);
    bandL.setCutoff (sr, inBand); bandR.setCutoff (sr, inBand);
    const float gIn = 0.45f + 0.3f * diff, gAp1 = 0.35f + 0.35f * diff;
    for (int k = 0; k < 4; ++k) { diffL[k].g = k < 2 ? gIn : gIn * 0.85f; diffR[k].g = diffL[k].g; }
    erApL.g = 0.45f; erApR.g = 0.45f;
    const float depth = motion * 1.1f * ms;   // up to ~1.1 ms random excursion (Lexicon-style chorus)
    float g[kSections], gLow[kSections];
    for (int j = 0; j < kSections; ++j)
    {
        auto& s = sec[j];
        const double seg = (kHaAp1[j] + kHaD1[j] + kHaAp2[j] + kHaD2[j]) * ms * sizeSm + depth;
        g[j] = (float) std::pow (10.0, -3.0 * seg / (sr * t60));
        gLow[j] = (float) std::min (0.99995, std::pow (10.0, -3.0 * seg / (sr * t60 * lowRatio)));
        if (frozen) { g[j] = std::min (g[j], 0.99999f); gLow[j] = g[j]; s.damp.setOpen(); }
        else s.damp.setDecayDamping (sr, p[HaTone], seg, t60, 1.0);
        s.low.setCutoff (sr, 350.0);
        s.ap1.g = j & 1 ? -gAp1 : gAp1;
        s.ap2.g = j & 1 ? 0.5f : -0.5f;
        s.spin.setRate (p[HaRate] * (0.77 + 0.19 * j), sr);
        s.wander.set (p[HaRate] * (0.9 + 0.23 * j), sr);
    }
    const float sizeCoef = onePoleCoef (0.35f, sr);
    const float erGain = early * 0.55f, lateGain = 0.42f;

    for (int i = 0; i < n; ++i)
    {
        sizeSm += sizeCoef * (sizeT - sizeSm);
        const float S = sizeSm * ms, erS = (0.25f + 0.75f * sizeSm) * ms;
        const float inGain = guard.nextInputGain (frozen, ctx.overdub);
        pre.push (hpL.process (inL[i]) * inGain);
        preR.push (hpR.process (inR[i]) * inGain);
        float xl = bandL.process (pre.readLinear (preS)), xr = bandR.process (preR.readLinear (preS));

        // early reflections (stereo pattern, a little cross-feed), smoothed by one short allpass each
        erL.push (xl); erR.push (xr);
        float el = 0.0f, er = 0.0f;
        for (int k = 0; k < kEarly; ++k)
        {
            const float tl = std::max (1.0f, kErL[k] * erS), tr = std::max (1.0f, kErR[k] * erS);
            el += kErG[k] * (k % 4 == 3 ? erR.readLinear (tl) : erL.readLinear (tl));
            er += kErG[k] * (k % 4 == 1 ? erL.readLinear (tr) : erR.readLinear (tr));
        }
        el = erApL.process (el, 2.9f * ms); er = erApR.process (er, 3.7f * ms);

        // late: input diffusion then the ring (fed from the direct path plus a little of the reflections)
        float ll = xl + 0.2f * el, lr = xr + 0.2f * er;
        for (int k = 0; k < 4; ++k)
        {
            const float sc = 0.6f + 0.4f * sizeSm;
            ll = diffL[k].process (ll, kHaDiff[k] * ms * sc);
            lr = diffR[k].process (lr, kHaDiff[k] * 1.13f * ms * sc);
        }
        float energy = 0.0f;
        float prevOut[kSections];
        for (int j = 0; j < kSections; ++j) prevOut[j] = sec[j].out;
        for (int j = 0; j < kSections; ++j)
        {
            auto& s = sec[j];
            float u = prevOut[(j + kSections - 1) % kSections];
            if (j == 0) u += ll;
            else if (j == 2) u += lr;
            const float mod = depth * (1.0f + 0.65f * s.wander.next() + 0.35f * s.spin.next());
            float a = s.ap1.process (u, kHaAp1[j] * S + mod);
            s.d1.push (a);
            float b = s.d1.readCubic (std::max (2.0f, kHaD1[j] * S));
            b = s.damp.process (b);
            const float lo = s.low.process (b);
            b = g[j] * (b - lo) + gLow[j] * lo;
            b = s.ap2.process (b, kHaAp2[j] * S);
            s.d2.push (b);
            s.out = s.d2.readCubic (std::max (2.0f, kHaD2[j] * S));
            energy += s.out * s.out;
        }
        const float gg = guard.update (frozen, energy, (int) (0.4 * sr));
        if (gg < 1.0f) for (auto& s : sec) s.out *= gg * 0.999f;

        auto tap = [] (const DelayLine& d, float len) { return d.readLinear (std::max (1.0f, len)); };
        const float yl = tap (sec[0].d1, kHaD1[0] * 0.31f * S) - tap (sec[1].d2, kHaD2[1] * 0.67f * S) + tap (sec[2].d1, kHaD1[2] * 0.53f * S)
                       - tap (sec[3].ap2.line, kHaAp2[3] * 0.41f * S) + tap (sec[2].d2, kHaD2[2] * 0.21f * S) - tap (sec[1].d1, kHaD1[1] * 0.77f * S)
                       + tap (sec[3].d1, kHaD1[3] * 0.13f * S);
        const float yr = tap (sec[2].d1, kHaD1[2] * 0.29f * S) - tap (sec[3].d2, kHaD2[3] * 0.71f * S) + tap (sec[0].d1, kHaD1[0] * 0.57f * S)
                       - tap (sec[1].ap2.line, kHaAp2[1] * 0.37f * S) + tap (sec[0].d2, kHaD2[0] * 0.23f * S) - tap (sec[3].d1, kHaD1[3] * 0.81f * S)
                       + tap (sec[1].d1, kHaD1[1] * 0.17f * S);
        outL[i] = yl * lateGain + el * erGain;
        outR[i] = yr * lateGain + er * erGain;
    }
}

// ============================================================================ Shimmer
float ShimmerUnit::Shifter::process (float x, float ratio) noexcept
{
    line.push (x);
    // two read heads half a window apart; the delay ramps at (1 - ratio) per sample and wraps under a sin^2 window
    phase += (double) (1.0f - ratio) / (double) window;
    phase -= std::floor (phase);
    float y = 0.0f;
    for (int k = 0; k < 2; ++k)
    {
        double ph = phase + 0.5 * k; ph -= std::floor (ph);
        const float d = 2.0f + (float) ph * window;
        const float w = std::sin ((float) ph * kPi);
        y += line.readCubic (d) * w * w;
    }
    return y;
}

void ShimmerUnit::prepare (double sampleRate)
{
    sr = sampleRate;
    for (int c = 0; c < 2; ++c)
    {
        sh[c].window = (float) ((c == 0 ? 0.071 : 0.083) * sr);
        sh[c].line.allocate ((int) (sh[c].window + 16));
        hp[c].highpass (sr, 260.0, 0.6);
        lp[c].lowpass (sr, 8500.0, 0.6);
    }
    envA = onePoleCoef (0.005f, sr);
    envR = onePoleCoef (0.25f, sr);
    reset();
}

void ShimmerUnit::reset() noexcept
{
    for (int c = 0; c < 2; ++c) { sh[c].line.clear(); sh[c].phase = 0.37 * c; hp[c].reset(); lp[c].reset(); }
    env = 0.0f; amountSm = 0.0f;
}

void ShimmerUnit::process (float amount, float semitones, const float* revL, const float* revR, float* fbL, float* fbR, int n) noexcept
{
    const float ratio = std::exp2 (semitones / 12.0f);
    const float step = 1.0f / (0.05f * (float) sr);
    for (int i = 0; i < n; ++i)
    {
        amountSm += clampf (amount - amountSm, -step, step);
        if (amountSm <= 1.0e-4f && env < 1.0e-6f) { fbL[i] = fbR[i] = 0.0f; continue; }
        float l = lp[0].process (hp[0].process (sh[0].process (revL[i], ratio)));
        float r = lp[1].process (hp[1].process (sh[1].process (revR[i], ratio)));
        l *= 0.6f * amountSm; r *= 0.6f * amountSm;
        // level guard: the shimmer feed never exceeds about -10 dBFS however long the reverb is
        const float a = std::max (std::abs (l), std::abs (r));
        env += (a > env ? envA : envR) * (a - env);
        const float lim = env > 0.3f ? 0.3f / env : 1.0f;
        fbL[i] = l * lim; fbR[i] = r * lim;
    }
}
} // namespace pa
