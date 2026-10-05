#include "HarmonyUnit.h"

namespace pa
{
void HarmonyUnit::prepare (double sampleRate, int maxLatencySamples)
{
    sr = sampleRate;
    classic.prepare (sr);
    fftVoc.prepare (sr, 8192);
    resonator.prepare (sr);
    shift.prepare (sr);
    ord.allocate (maxLatencySamples + 64);
    padCur.allocate (maxLatencySamples + 64);
    padPrev.allocate (maxLatencySamples + 64);
    fadeLen = std::max (1, (int) (0.05 * sr));
    reset();
}

void HarmonyUnit::reset() noexcept
{
    classic.reset(); fftVoc.reset(); resonator.reset(); shift.reset();
    ord.clear(); padCur.clear(); padPrev.clear();
    fadePos = fadeLen;
    idle = false;
}

int HarmonyUnit::methodLatency (int m, int quality) const noexcept
{
    switch (m)
    {
        case MethodFft: return FftVocoder::sizeForQuality (quality, sr);
        case MethodShift: return shift.latencyForQuality (quality);
        default: return 0;
    }
}

void HarmonyUnit::resetMethod (int m) noexcept
{
    switch (m)
    {
        case MethodClassic: classic.reset(); break;
        case MethodFft: fftVoc.reset(); break;
        case MethodResonator: resonator.reset(); break;
        case MethodShift: shift.reset(); break;
        default: break;
    }
}

void HarmonyUnit::runMethod (int m, const HarmonyContext& ctx, const float* inL, const float* inR, float* oL, float* oR, int n) noexcept
{
    switch (m)
    {
        case MethodClassic: classic.process (ctx, inL, inR, oL, oR, n); break;
        case MethodFft: fftVoc.process (ctx, inL, inR, oL, oR, n); break;
        case MethodResonator: resonator.process (ctx, inL, inR, oL, oR, n); break;
        case MethodShift: shift.process (ctx, inL, inR, oL, oR, n); break;
        default:
            std::memcpy (oL, inL, sizeof (float) * (size_t) n);
            std::memcpy (oR, inR, sizeof (float) * (size_t) n);
            break;
    }
}

void HarmonyUnit::process (const HarmonyContext& ctx, int method, bool studio, int studioLatency,
                           const float* inL, const float* inR, float* outL, float* outR, int n, float dStart, float dEnd) noexcept
{
    const int q = ctx.quality;
    if (q != lastQuality) { lastQuality = q; }

    // method switching (bounded to two simultaneous states; newer requests supersede)
    if (method != cur)
    {
        if (fadePos < fadeLen && method == prev)
        {
            std::swap (cur, prev);
            std::swap (padCur, padPrev);
            fadePos = fadeLen - fadePos;
        }
        else
        {
            if (fadePos >= fadeLen) { prev = cur; std::swap (padCur, padPrev); }
            else padCur.clear(); // superseded target: drop its short history
            cur = method;
            resetMethod (cur);
            fadePos = 0;
        }
    }
    const bool fading = fadePos < fadeLen;

    // CPU: skip harmony processing while depth stays at zero and nothing is fading
    const bool silentDepth = dStart <= 0.0f && dEnd <= 0.0f && ! fading;
    if (silentDepth && ! idle) idle = true;
    else if (! silentDepth && idle) { idle = false; resetMethod (cur); }

    const int latCur = methodLatency (cur, q), latPrev = methodLatency (prev, q);
    const int Lcur = studio ? studioLatency : latCur;
    const int Lprev = studio ? studioLatency : latPrev;
    lastLatency = Lcur;

    if (! idle)
    {
        runMethod (cur, ctx, inL, inR, aL, aR, n);
        if (fading) runMethod (prev, ctx, inL, inR, bL, bR, n);
    }

    const int padC = std::max (0, Lcur - latCur), padP = std::max (0, Lprev - latPrev);
    const float dStep = (dEnd - dStart) / (float) std::max (1, n);
    for (int i = 0; i < n; ++i)
    {
        ord.push (inL[i], inR[i]);
        float oL, oR;
        float hL = 0.0f, hR = 0.0f;
        if (fading)
        {
            const float t = (float) fadePos / (float) fadeLen;
            oL = (1.0f - t) * ord.readL (Lprev) + t * ord.readL (Lcur);
            oR = (1.0f - t) * ord.readR (Lprev) + t * ord.readR (Lcur);
            padCur.push (aL[i], aR[i]);
            padPrev.push (bL[i], bR[i]);
            hL = t * padCur.readL (padC) + (1.0f - t) * padPrev.readL (padP);
            hR = t * padCur.readR (padC) + (1.0f - t) * padPrev.readR (padP);
            if (++fadePos >= fadeLen) { resetMethod (prev); padPrev.clear(); }
        }
        else
        {
            oL = ord.readL (Lcur);
            oR = ord.readR (Lcur);
            if (! idle)
            {
                padCur.push (aL[i], aR[i]);
                hL = padCur.readL (padC);
                hR = padCur.readR (padC);
            }
        }
        const float d = dStart + dStep * (float) i;
        if (cur == MethodOff && ! fading) { outL[i] = oL; outR[i] = oR; continue; }
        outL[i] = (1.0f - d) * oL + d * hL;
        outR[i] = (1.0f - d) * oR + d * hR;
    }
}
} // namespace pa
