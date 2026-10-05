#include "Resonator.h"

namespace pa
{
void Resonator::reset() noexcept
{
    for (auto& v : voice)
    {
        for (auto& pt : v.part) { pt.lr = pt.li = pt.rr = pt.ri = 0.0f; }
        v.coefHz = -1.0f;
        v.x1L = v.x1R = 0.0f;
    }
}

void Resonator::updateCoefs (Voice& v, float f0, const ParamSet& p, int cap) noexcept
{
    const int P = std::clamp (std::min (p.i (RsHarmonics), cap), 1, kMaxPartials);
    const float T = std::max (0.05f, p[RsDecay]);
    const float damp = p[RsDamping] / 100.0f;
    const float spread = p[RsSpread] / 100.0f;
    const float tilt = 1.3f - (p[Colour] / 100.0f) * 1.1f;
    float sumA2 = 0.0f;
    float amps[kMaxPartials];
    for (int k = 0; k < P; ++k)
    {
        const float a = std::pow ((float) (k + 1), -tilt);
        amps[k] = a;
        if ((k + 1) * f0 < sr * 0.45) sumA2 += a * a;
    }
    const float norm = sumA2 > 0.0f ? 1.0f / std::sqrt (sumA2) : 0.0f;
    v.count = P;
    for (int k = 0; k < P; ++k)
    {
        auto& pt = v.part[k];
        const double f = (double) f0 * (k + 1);
        if (f >= sr * 0.45 || f0 <= 0.0f)
        {
            pt.b = 0.0f; pt.pr = 0.0f; pt.pi = 0.0f;
            pt.lr = pt.li = pt.rr = pt.ri = 0.0f;
            continue;
        }
        const float Tk = T / (1.0f + 4.0f * damp * (float) k / (float) std::max (1, P - 1));
        const double r = std::exp (-6.907755 / (Tk * sr));
        const double w = 2.0 * kPiD * f / sr;
        pt.pr = (float) (r * std::cos (w));
        pt.pi = (float) (r * std::sin (w));
        pt.b = (float) std::sqrt (1.0 - r * r) * amps[k] * norm * 1.41f;
        const float pan = k == 0 ? 0.0f : ((k & 1) ? -spread : spread) * 0.85f;
        panGains (pan, pt.gl, pt.gr);
    }
}

void Resonator::process (const HarmonyContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    const ParamSet& p = *ctx.p;
    const int cap = partialCap (ctx.quality);
    const float key = p[RsHarmonics] * 1.0e-1f + p[RsDecay] * 7.3f + p[RsDamping] * 0.37f + p[RsSpread] * 0.11f + p[Colour] * 0.013f + (float) cap * 31.0f;
    const bool paramsChanged = key != paramKey;
    paramKey = key;
    const bool enhanced = p.i (RsExcite) == 1;

    for (int i = 0; i < n; ++i) { outL[i] = 0.0f; outR[i] = 0.0f; }

    for (int vi = 0; vi < kMaxVoices; ++vi)
    {
        const auto& slot = ctx.voices->slot (vi);
        Voice& v = voice[vi];
        if (slot.retuned)
        {
            for (auto& pt : v.part) { pt.lr = pt.li = pt.rr = pt.ri = 0.0f; }
            v.coefHz = -1.0f;
        }
        const float f0 = ctx.voices->hz[vi][0];
        if (slot.note < 0 && v.coefHz < 0.0f) continue;
        if (f0 > 0.0f && (paramsChanged || std::abs (f0 - v.coefHz) > v.coefHz * 1.0e-4f || v.coefHz < 0.0f))
        {
            updateCoefs (v, f0, p, cap);
            v.coefHz = f0;
        }
        if (v.coefHz < 0.0f) continue;
        const float* g = ctx.voices->gain[vi];
        const float* fade = ctx.voices->outFade[vi];
        float x1L = v.x1L, x1R = v.x1R;
        for (int i = 0; i < n; ++i)
        {
            float xl = inL[i], xr = inR[i];
            if (enhanced)
            {
                const float el = xl - 0.82f * x1L, er = xr - 0.82f * x1R;
                x1L = xl; x1R = xr;
                xl = fastTanh (2.2f * el) * 0.75f;
                xr = fastTanh (2.2f * er) * 0.75f;
            }
            xl *= g[i]; xr *= g[i];
            float yl = 0.0f, yr = 0.0f;
            for (int k = 0; k < v.count; ++k)
            {
                auto& pt = v.part[k];
                const float lr = pt.pr * pt.lr - pt.pi * pt.li + pt.b * xl;
                const float li = pt.pr * pt.li + pt.pi * pt.lr;
                const float rr = pt.pr * pt.rr - pt.pi * pt.ri + pt.b * xr;
                const float ri = pt.pr * pt.ri + pt.pi * pt.rr;
                pt.lr = lr; pt.li = li; pt.rr = rr; pt.ri = ri;
                yl += lr * pt.gl;
                yr += rr * pt.gr;
            }
            outL[i] += yl * fade[i];
            outR[i] += yr * fade[i];
        }
        v.x1L = x1L; v.x1R = x1R;
        // once a voice is free and fully decayed, stop computing it
        if (slot.note < 0)
        {
            float e = 0.0f;
            for (int k = 0; k < v.count; ++k) e += std::abs (v.part[k].lr) + std::abs (v.part[k].rr);
            if (e < 1.0e-7f) { for (auto& pt : v.part) { pt.lr = pt.li = pt.rr = pt.ri = 0.0f; } v.coefHz = -1.0f; }
        }
    }
    // denormal-safe flush of tiny states is handled by FTZ; additionally keep output finite
}
} // namespace pa
