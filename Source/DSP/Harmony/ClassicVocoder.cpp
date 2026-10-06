#include "ClassicVocoder.h"

namespace pa
{
void ClassicVocoder::prepare (double sampleRate)
{
    sr = sampleRate;
    bands = 0;
    reset();
}

void ClassicVocoder::reset() noexcept
{
    for (auto& b : band)
    {
        for (int k = 0; k < 2; ++k) { b.mL[k].reset(); b.mR[k].reset(); b.c[k].reset(); }
        b.envL = b.envR = b.envC = b.envG = 0.0f;
    }
    for (auto& h : noiseHpL) h.reset();
    for (auto& h : noiseHpR) h.reset();
}

void ClassicVocoder::configure (int numBands, int ord, const ParamSet& p) noexcept
{
    bands = std::clamp (numBands, 4, kMaxBands);
    order = ord;
    const double fLo = 90.0, fHi = std::min (12000.0, sr * 0.45);
    bwOct = (float) (std::log2 (fHi / fLo) / (bands - 1));
    // Each 2nd-order stage gets a slightly wider bandwidth when cascaded so adjacent bands still cross near -3..-6 dB.
    const double stageBw = bwOct * (order == 4 ? 1.55 : 1.05);
    const double q = 1.0 / (2.0 * std::sinh (std::log (2.0) / 2.0 * stageBw));
    for (int b = 0; b < bands; ++b)
    {
        fc[b] = (float) (fLo * std::exp2 (b * (double) bwOct));
        for (int k = 0; k < 2; ++k)
        {
            band[b].mL[k].bandpass (sr, fc[b], q);
            band[b].mR[k].bandpass (sr, fc[b], q);
            band[b].c[k].bandpass (sr, fc[b], q);
        }
    }
    for (int k = 0; k < 2; ++k) { noiseHpL[k].highpass (sr, 4000.0); noiseHpR[k].highpass (sr, 4000.0); }
    // Calibrated so a broadband modulator through a held chord gives roughly the modulator's level (see Tests).
    calib = (order == 4 ? 5.2f : 3.6f) * std::sqrt (32.0f / (float) bands);
    lastColour = -1.0f;
    (void) p;
    reset();
}

void ClassicVocoder::process (const HarmonyContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    const ParamSet& p = *ctx.p;
    static const int bandChoices[3] = { 24, 32, 48 };
    const int wantBands = bandChoices[std::clamp (p.i (ClBands), 0, 2)];
    const int wantOrder = ctx.quality == 0 ? 2 : 4;
    if (wantBands != bands || wantOrder != order) configure (wantBands, wantOrder, p);

    if (p[ClAttack] != lastAttack || p[ClRelease] != lastRelease)
    {
        lastAttack = p[ClAttack]; lastRelease = p[ClRelease];
        atk = onePoleCoef (lastAttack / 1000.0f, sr);
        rel = onePoleCoef (lastRelease / 1000.0f, sr);
        catk = onePoleCoef (0.002f, sr);
        crel = onePoleCoef (0.04f, sr);
    }
    const float colour = p[Colour] / 100.0f;
    if (colour != lastColour)
    {
        lastColour = colour;
        const float t = (colour - 0.5f) * 1.0f; // -0.5 .. +0.5 => about -3 .. +3 dB/oct
        for (int b = 0; b < bands; ++b) tiltGain[b] = std::pow (fc[b] / 1000.0f, t);
    }
    const bool link = p.b (ClStereoLink);
    const float shiftBands = p[ClFormant] / 12.0f / bwOct;
    const float noiseAmt = p[ClNoise] / 100.0f;
    const int stages = order == 4 ? 2 : 1;
    constexpr float kMaxBoost = 8.0f;

    // Pass 1: modulator envelopes
    for (int b = 0; b < bands; ++b)
    {
        Band& bd = band[b];
        float eL = bd.envL, eR = bd.envR;
        for (int i = 0; i < n; ++i)
        {
            float l = inL[i], r = inR[i];
            for (int s = 0; s < stages; ++s) { l = bd.mL[s].process (l); r = bd.mR[s].process (r); }
            const float al = std::abs (l), ar = std::abs (r);
            eL += (al > eL ? atk : rel) * (al - eL);
            eR += (ar > eR ? atk : rel) * (ar - eR);
            if (link) { const float m = 0.5f * (eL + eR); envBufL[b][i] = m; envBufR[b][i] = m; }
            else { envBufL[b][i] = eL; envBufR[b][i] = eR; }
        }
        bd.envL = eL; bd.envR = eR;
    }

    for (int i = 0; i < n; ++i) { outL[i] = 0.0f; outR[i] = 0.0f; }

    // visualization tap: which synthesis band is nearest each sounding voice's fundamental
    int laneOfBand[kMaxBands];
    for (int b = 0; b < bands; ++b) laneOfBand[b] = -1;
    if (ctx.tap != nullptr && ctx.voices != nullptr)
        for (int v = 0; v < kMaxVoices; ++v)
        {
            const auto& sl = ctx.voices->slot (v);
            if (sl.note < 0 || sl.env <= 1.0e-4f) continue;
            const float f = 440.0f * std::exp2 (sl.logHz);
            int best = -1; float bestD = 1.0e9f;
            for (int b = 0; b < bands; ++b) { const float d = std::abs (std::log2 (fc[b] / f)); if (d < bestD) { bestD = d; best = b; } }
            if (best >= 0 && laneOfBand[best] < 0) laneOfBand[best] = v;
        }

    // Pass 2: synthesis
    for (int b = 0; b < bands; ++b)
    {
        Band& bd = band[b];
        // formant shift: synthesis band b reads modulator band (b - shift)
        const float src = (float) b - shiftBands;
        int i0 = (int) std::floor (src);
        const float fr = src - (float) i0;
        const bool valid0 = i0 >= 0 && i0 < bands, valid1 = i0 + 1 >= 0 && i0 + 1 < bands;
        const float tg = tiltGain[b] * calib;
        float eC = bd.envC, eG = bd.envG;
        const int lane = laneOfBand[b];
        float tapMn = 0.0f, tapMx = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float c = ctx.carrier[i];
            for (int s = 0; s < stages; ++s) c = bd.c[s].process (c);
            const float ac = std::abs (c);
            eC += (ac > eC ? catk : crel) * (ac - eC);
            const float g = ctx.carrierGain[i];
            eG += (g > eG ? catk : crel) * (g - eG);
            // whitening: carrier band normalised to the voice gain (scale invariant -> follows note envelopes)
            const float norm = std::min (kMaxBoost, eG * 0.5f / (eC + 1.0e-6f + 1.0e-4f * eG));
            float mL = 0.0f, mR = 0.0f;
            if (valid0) { mL += (1.0f - fr) * envBufL[i0][i]; mR += (1.0f - fr) * envBufR[i0][i]; }
            if (valid1) { mL += fr * envBufL[i0 + 1][i]; mR += fr * envBufR[i0 + 1][i]; }
            const float cg = c * norm * tg;
            outL[i] += cg * mL;
            outR[i] += cg * mR;
            if (lane >= 0)
            {
                const float t = 0.5f * cg * (mL + mR);
                tapMn = std::min (tapMn, t); tapMx = std::max (tapMx, t);
            }
        }
        if (lane >= 0) { ctx.tap->mn[lane] = std::min (ctx.tap->mn[lane], tapMn); ctx.tap->mx[lane] = std::max (ctx.tap->mx[lane], tapMx); }
        bd.envC = eC; bd.envG = eG;
    }

    if (noiseAmt > 0.0f)
    {
        for (int i = 0; i < n; ++i)
        {
            float l = inL[i], r = inR[i];
            for (int s = 0; s < 2; ++s) { l = noiseHpL[s].process (l); r = noiseHpR[s].process (r); }
            outL[i] += l * noiseAmt;
            outR[i] += r * noiseAmt;
        }
    }
}
} // namespace pa
