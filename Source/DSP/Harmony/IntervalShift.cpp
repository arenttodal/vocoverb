#include "IntervalShift.h"

namespace pa
{
void IntervalShift::prepare (double sampleRate)
{
    sr = sampleRate;
    const int maxWin = (int) std::ceil (0.09 * sr);
    bufL.allocate (maxWin + 16);
    bufR.allocate (maxWin + 16);
    envCoef = onePoleCoef (0.012f, sr);
    gainCoef = onePoleCoef (0.03f, sr);
    quality = -1;
    configure (1);
}

void IntervalShift::configure (int q) noexcept
{
    quality = q;
    winLen = std::max (64, (int) std::round (windowMsForQuality (q) * 0.001 * sr));
    if (winLen & 1) ++winLen;
    dmin = 2;
    latency = dmin + winLen / 2;
    const double fLo = 120.0, fHi = std::min (9000.0, sr * 0.42);
    for (int b = 0; b < kBands; ++b)
    {
        const double f = fLo * std::pow (fHi / fLo, (double) b / (kBands - 1));
        refBpL[b].bandpass (sr, f, 1.6); refBpR[b].bandpass (sr, f, 1.6);
        outBpL[b].bandpass (sr, f, 1.6); outBpR[b].bandpass (sr, f, 1.6);
    }
    reset();
}

void IntervalShift::reset() noexcept
{
    bufL.clear(); bufR.clear();
    for (int v = 0; v < kMaxVoices; ++v) { voice[v].phase = 0.25 * v / kMaxVoices; voice[v].idW = 1.0f; }
    toneL.reset(); toneR.reset();
    for (int b = 0; b < kBands; ++b)
    {
        refBpL[b].reset(); refBpR[b].reset(); outBpL[b].reset(); outBpR[b].reset();
        envRefL[b] = envRefR[b] = envOutL[b] = envOutR[b] = 0.0f;
        gainL[b] = gainR[b] = 1.0f;
    }
}

void IntervalShift::process (const HarmonyContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    const ParamSet& p = *ctx.p;
    if (ctx.quality != quality) configure (ctx.quality);
    const float refHz = midiToHz ((float) p.i (ShRef), p[RefTuning]);
    const int limit = p.i (ShLimit);
    const float spread = p[ShSpread] / 100.0f;
    const float W = (float) winLen;
    const float idCoef = onePoleCoef (0.015f, sr);
    const bool toneBypass = p[Colour] >= 99.5f;
    toneL.setCutoff (sr, 1800.0 * std::exp2 (p[Colour] / 100.0 * 3.5));
    toneR.a = toneL.a;
    const bool formant = p.b (ShFormant);

    // fold factor per voice computed at chunk start (keeps |shift| <= limit by octaves)
    float fold[kMaxVoices];
    for (int v = 0; v < kMaxVoices; ++v)
    {
        fold[v] = 1.0f;
        const float hz0 = ctx.voices->hz[v][0];
        if (hz0 <= 0.0f) continue;
        float semis = 12.0f * std::log2 (hz0 / refHz);
        float adj = 0.0f;
        if (limit < 12) { const float r = std::round (semis / 12.0f) * 12.0f; adj = -r; semis -= r; if (std::abs (semis) > (float) limit) { adj -= semis; semis = 0; } }
        else { while (semis + adj > (float) limit) adj -= 12.0f; while (semis + adj < -(float) limit) adj += 12.0f; }
        fold[v] = std::exp2 (adj / 12.0f);
        const float pan = spread * ((float) v - 2.5f) / 2.5f;
        panGains (pan, voice[v].gl, voice[v].gr);
    }

    for (int i = 0; i < n; ++i)
    {
        bufL.push (inL[i]);
        bufR.push (inR[i]);
        float yl = 0.0f, yr = 0.0f;
        for (int v = 0; v < kMaxVoices; ++v)
        {
            const float g = ctx.voices->gain[v][i];
            Voice& vc = voice[v];
            if (g <= 1.0e-7f) continue;
            const float hz = ctx.voices->hz[v][i];
            if (hz <= 0.0f) continue;
            const float ratio = hz / refHz * fold[v];
            const float idTarget = std::abs (ratio - 1.0f) < 5.0e-4f ? 1.0f : 0.0f;
            vc.idW += idCoef * (idTarget - vc.idW);
            float sl = 0.0f, sr2 = 0.0f;
            if (vc.idW < 0.999f)
            {
                vc.phase += (1.0 - ratio) / W;
                vc.phase -= std::floor (vc.phase);
                const float ph = (float) vc.phase;
                float ph2 = ph + 0.5f; if (ph2 >= 1.0f) ph2 -= 1.0f;
                const float d1 = (float) dmin + ph * W, d2 = (float) dmin + ph2 * W;
                const float s1 = std::sin (kPi * ph);
                const float w1 = s1 * s1, w2 = 1.0f - w1;
                sl = w1 * bufL.readCubic (d1) + w2 * bufL.readCubic (d2);
                sr2 = w1 * bufR.readCubic (d1) + w2 * bufR.readCubic (d2);
            }
            if (vc.idW > 0.001f)
            {
                const float il = bufL.readInt (latency + 1), ir = bufR.readInt (latency + 1);
                sl = vc.idW * il + (1.0f - vc.idW) * sl;
                sr2 = vc.idW * ir + (1.0f - vc.idW) * sr2;
            }
            yl += sl * g * vc.gl;
            yr += sr2 * g * vc.gr;
        }
        if (! toneBypass) { yl = toneL.process (yl); yr = toneR.process (yr); }

        if (formant)
        {
            const float rl = bufL.readInt (latency + 1), rr = bufR.readInt (latency + 1);
            float corrL = 0.0f, corrR = 0.0f;
            for (int b = 0; b < kBands; ++b)
            {
                const float a = refBpL[b].process (rl), c = refBpR[b].process (rr);
                const float ol = outBpL[b].process (yl), orr = outBpR[b].process (yr);
                envRefL[b] += envCoef * (std::abs (a) - envRefL[b]);
                envRefR[b] += envCoef * (std::abs (c) - envRefR[b]);
                envOutL[b] += envCoef * (std::abs (ol) - envOutL[b]);
                envOutR[b] += envCoef * (std::abs (orr) - envOutR[b]);
                const float tl = clampf (envRefL[b] / (envOutL[b] + 1.0e-5f), 0.25f, 4.0f);
                const float tr = clampf (envRefR[b] / (envOutR[b] + 1.0e-5f), 0.25f, 4.0f);
                gainL[b] += gainCoef * (tl - gainL[b]);
                gainR[b] += gainCoef * (tr - gainR[b]);
                corrL += ol * (gainL[b] - 1.0f);
                corrR += orr * (gainR[b] - 1.0f);
            }
            yl += corrL * 0.55f;
            yr += corrR * 0.55f;
        }
        outL[i] = yl;
        outR[i] = yr;
    }
}
} // namespace pa
