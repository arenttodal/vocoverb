#include "FftVocoder.h"

namespace pa
{
void FftVocoder::prepare (double sampleRate, int maxFftSize)
{
    sr = sampleRate;
    maxN = 1;
    while (maxN < maxFftSize) maxN <<= 1;
    fft.prepare (maxN);
    const size_t N = (size_t) maxN;
    win.assign (N, 0.0f);
    inL.assign (N, 0.0f); inR.assign (N, 0.0f); inC.assign (N, 0.0f);
    accL.assign (N, 0.0f); accR.assign (N, 0.0f);
    readyL.assign (N, 0.0f); readyR.assign (N, 0.0f);
    zMod.assign (N, {}); zCar.assign (N, {});
    specL.assign (N / 2 + 2, {}); specR.assign (N / 2 + 2, {});
    tiltTab.assign (N / 2 + 2, 1.0f); tiltKey = -100.0f;
    envL.assign (N / 2 + 2, 0.0f); envR.assign (N / 2 + 2, 0.0f); envC.assign (N / 2 + 2, 0.0f);
    prevL.assign (N / 2 + 2, 0.0f); prevR.assign (N / 2 + 2, 0.0f);
    csL.assign (N / 2 + 3, 0.0); csR.assign (N / 2 + 3, 0.0); csC.assign (N / 2 + 3, 0.0);
    configure (std::min (maxN, sizeForQuality (1, sr)));
}

void FftVocoder::configure (int n) noexcept
{
    fftSize = std::clamp (n, 64, maxN);
    hop = fftSize / 4;
    for (int j = 0; j < fftSize; ++j)
        win[(size_t) j] = (float) std::sqrt (0.5 - 0.5 * std::cos (2.0 * kPiD * j / fftSize));
    reset();
}

void FftVocoder::reset() noexcept
{
    stage = 0;
    std::fill (inL.begin(), inL.end(), 0.0f); std::fill (inR.begin(), inR.end(), 0.0f); std::fill (inC.begin(), inC.end(), 0.0f);
    std::fill (accL.begin(), accL.end(), 0.0f); std::fill (accR.begin(), accR.end(), 0.0f);
    std::fill (readyL.begin(), readyL.end(), 0.0f); std::fill (readyR.begin(), readyR.end(), 0.0f);
    std::fill (prevL.begin(), prevL.end(), 0.0f); std::fill (prevR.begin(), prevR.end(), 0.0f);
    fill = 0; gainAccum = 0.0f; frameGain = 0.0f;
}

void FftVocoder::process (const HarmonyContext& ctx, const float* xl, const float* xr, float* outL, float* outR, int n) noexcept
{
    const int want = std::min (maxN, sizeForQuality (ctx.quality, sr));
    if (want != fftSize) configure (want);
    const int base = fftSize - hop;
    for (int i = 0; i < n; ++i)
    {
        inL[(size_t) (base + fill)] = xl[i];
        inR[(size_t) (base + fill)] = xr[i];
        inC[(size_t) (base + fill)] = ctx.carrier[i];
        gainAccum += ctx.carrierGain[i];
        outL[i] = readyL[(size_t) fill];
        outR[i] = readyR[(size_t) fill];
        ++fill;
        // spread the previous frame's work over this hop (keeps per-block cost flat)
        while (stage > 0 && stage <= kStages && fill * kStages >= stage * hop) runStage (ctx);
        if (fill >= hop)
        {
            while (stage > 0 && stage <= kStages) runStage (ctx);
            if (stage > kStages) finishFrame();
            beginFrame();
            fill = 0;
        }
    }
}

void FftVocoder::beginFrame() noexcept
{
    const int N = fftSize;
    frameGain = std::min (1.0f, gainAccum / (float) hop);
    gainAccum = 0.0f;
    for (int j = 0; j < N; ++j)
    {
        const float w = win[(size_t) j];
        zMod[(size_t) j] = { inL[(size_t) j] * w, inR[(size_t) j] * w };
        zCar[(size_t) j] = { inC[(size_t) j] * w, 0.0f };
    }
    const int hp = hop;
    std::memmove (inL.data(), inL.data() + hp, sizeof (float) * (size_t) (N - hp));
    std::memmove (inR.data(), inR.data() + hp, sizeof (float) * (size_t) (N - hp));
    std::memmove (inC.data(), inC.data() + hp, sizeof (float) * (size_t) (N - hp));
    stage = 1;
}

void FftVocoder::finishFrame() noexcept
{
    const int N = fftSize;
    const float scale = 1.0f / (2.0f * (float) N);
    for (int j = 0; j < N; ++j)
    {
        const float w = win[(size_t) j] * scale;
        accL[(size_t) j] += zMod[(size_t) j].real() * w;
        accR[(size_t) j] += zMod[(size_t) j].imag() * w;
    }
    const int hp = hop;
    for (int j = 0; j < hp; ++j) { readyL[(size_t) j] = accL[(size_t) j]; readyR[(size_t) j] = accR[(size_t) j]; }
    std::memmove (accL.data(), accL.data() + hp, sizeof (float) * (size_t) (N - hp));
    std::memmove (accR.data(), accR.data() + hp, sizeof (float) * (size_t) (N - hp));
    std::fill (accL.begin() + (N - hp), accL.begin() + N, 0.0f);
    std::fill (accR.begin() + (N - hp), accR.begin() + N, 0.0f);
    stage = 0;
}

void FftVocoder::runStage (const HarmonyContext& ctx) noexcept
{
    const int N = fftSize, H = N / 2;
    const ParamSet& p = *ctx.p;
    auto* sL = specL.data();
    auto* sR = specR.data();
    const int st = stage++;
    const double binHz = sr / N;
    switch (st)
    {
        case 1: fft.forward (zMod.data(), N); return;
        case 2: if (! identityMode) fft.forward (zCar.data(), N); return;
        case 3:
        {
            // split the packed stereo spectrum, powers and prefix sums
            for (int k = 0; k <= H; ++k)
            {
                const auto zk = zMod[(size_t) k];
                const auto zn = std::conj (zMod[(size_t) ((N - k) & (N - 1))]);
                sL[k] = { 0.5f * (zk.real() + zn.real()), 0.5f * (zk.imag() + zn.imag()) };
                const float a = zk.real() - zn.real(), b = zk.imag() - zn.imag();
                sR[k] = { 0.5f * b, -0.5f * a };
            }
            if (identityMode) return;
            float lowF0 = 1.0e9f;
            for (int v = 0; v < kMaxVoices; ++v)
            {
                const auto& s = ctx.voices->slot (v);
                if (s.note >= 0 && s.env > 1.0e-4f) lowF0 = std::min (lowF0, ctx.voices->hz[v][0]);
            }
            if (lowF0 > 1.0e8f) lowF0 = 100.0f;
            minHalfC = std::max (2, (int) std::ceil (1.2 * lowF0 / binHz));
            csL[0] = csR[0] = csC[0] = 0.0;
            for (int k = 0; k <= H; ++k)
            {
                const auto c = zCar[(size_t) k];
                const float a = sL[k].real() * sL[k].real() + sL[k].imag() * sL[k].imag();
                const float b = sR[k].real() * sR[k].real() + sR[k].imag() * sR[k].imag();
                const float cc = c.real() * c.real() + c.imag() * c.imag();
                csL[(size_t) k + 1] = csL[(size_t) k] + a;
                csR[(size_t) k + 1] = csR[(size_t) k] + b;
                csC[(size_t) k + 1] = csC[(size_t) k] + cc;
            }
            return;
        }
        case 4:
        {
            if (identityMode) return;
            const float smooth = p[FftSmooth] / 100.0f;
            const double h = 1.0 / 24.0 + smooth * (1.0 / 3.0 - 1.0 / 24.0);
            const double hc = std::max (h, 0.5);
            const double up = std::exp2 (h), dn = std::exp2 (-h), upc = std::exp2 (hc), dnc = std::exp2 (-hc);
            const float persist = p[FftPersist] / 100.0f;
            const double hopSec = (double) (N / 4) / sr;
            const float pf = persist > 0.001f ? (float) std::exp (-hopSec / (0.02 + persist * 0.6)) : 0.0f;
            double meanC = 0.0;
            for (int k = 0; k <= H; ++k)
            {
                int lo = (int) (k * dn), hi = (int) std::ceil (k * up);
                if (hi - lo < 2) { lo = std::max (0, k - 1); hi = std::min (H, k + 1); }
                lo = std::max (0, lo); hi = std::min (H, hi);
                const double inv = 1.0 / (hi - lo + 1);
                float eL = (float) std::sqrt (std::max (0.0, (csL[(size_t) hi + 1] - csL[(size_t) lo]) * inv));
                float eR = (float) std::sqrt (std::max (0.0, (csR[(size_t) hi + 1] - csR[(size_t) lo]) * inv));
                eL = std::max (eL, prevL[(size_t) k] * pf);
                eR = std::max (eR, prevR[(size_t) k] * pf);
                prevL[(size_t) k] = eL; prevR[(size_t) k] = eR;
                envL[(size_t) k] = eL; envR[(size_t) k] = eR;
                int loc = (int) (k * dnc), hic = (int) std::ceil (k * upc);
                if (k - loc < minHalfC) loc = k - minHalfC;
                if (hic - k < minHalfC) hic = k + minHalfC;
                loc = std::max (0, loc); hic = std::min (H, hic);
                envC[(size_t) k] = (float) std::sqrt (std::max (0.0, (csC[(size_t) hic + 1] - csC[(size_t) loc]) / (double) (hic - loc + 1)));
                meanC += envC[(size_t) k];
            }
            meanEnvC = meanC / (H + 1);
            return;
        }
        case 5:
        {
            if (! identityMode)
            {
                const bool link = p.b (FftStereoLink);
                const float fRatio = std::exp2 (-p[FftFormant] / 12.0f);
                const float tilt = (p[Colour] / 100.0f - 0.5f);
                if (tilt != tiltKey || tiltN != N)
                {
                    tiltKey = tilt; tiltN = N;
                    for (int k = 0; k <= H; ++k) tiltTab[(size_t) k] = std::pow ((float) std::max (1, k) * (float) binHz / 1000.0f, tilt);
                }
                const float eps = (float) (1.0e-9 + 1.0e-3 * meanEnvC);
                constexpr float kMaxRatio = 40.0f;
                for (int k = 0; k <= H; ++k)
                {
                    float mL = 0.0f, mR = 0.0f;
                    const float src = (float) k * fRatio;
                    const int i0 = (int) src;
                    const float fr = src - (float) i0;
                    if (i0 + 1 <= H)
                    {
                        mL = envL[(size_t) i0] + fr * (envL[(size_t) i0 + 1] - envL[(size_t) i0]);
                        mR = envR[(size_t) i0] + fr * (envR[(size_t) i0 + 1] - envR[(size_t) i0]);
                    }
                    if (link) { const float m = 0.5f * (mL + mR); mL = mR = m; }
                    const float tg = tiltTab[(size_t) k] * frameGain;
                    const float ec = 1.0f / (envC[(size_t) k] + eps);
                    const float gL = std::min (kMaxRatio, mL * ec) * tg, gR = std::min (kMaxRatio, mR * ec) * tg;
                    const auto c = zCar[(size_t) k];
                    sL[k] = { c.real() * gL, c.imag() * gL };
                    sR[k] = { c.real() * gR, c.imag() * gR };
                }
            }
            // pack L + iR into one Hermitian-pair spectrum for a single inverse transform
            for (int k = 0; k <= H; ++k)
            {
                const auto yl = sL[k], yr = sR[k];
                if (k == 0 || k == H) { zMod[(size_t) k] = { yl.real(), yr.real() }; continue; }
                zMod[(size_t) k] = { yl.real() - yr.imag(), yl.imag() + yr.real() };
                zMod[(size_t) (N - k)] = { yl.real() + yr.imag(), -yl.imag() + yr.real() };
            }
            return;
        }
        case 6: fft.inverse (zMod.data(), N); return;
        default: return;
    }
}
} // namespace pa
