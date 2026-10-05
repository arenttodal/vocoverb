#include "Delays.h"

namespace pa
{
double syncedMs (int divIndex, double bpm) noexcept
{
    return divisionBeats (divIndex) * 60000.0 / std::max (20.0, bpm);
}

// ============================================================================ BBD
void BbdDelay::prepare (double sampleRate)
{
    sr = sampleRate;
    const int maxS = (int) std::ceil (2.2 * sr) + 64;
    lineL.allocate (maxS);
    lineR.allocate (maxS);
    clears.count = 0;
    clears.add (lineL); clears.add (lineR);
    envA = onePoleCoef (0.003f, sr);
    envRel = onePoleCoef (0.08f, sr);
    envRelSlow = onePoleCoef (0.14f, sr);
    drift.set (0.13, sr);
    reset();
}

void BbdDelay::resetSmallState() noexcept
{
    inLpL.reset(); inLpR.reset();
    for (int k = 0; k < 2; ++k) { fbLpL[k].reset(); fbLpR[k].reset(); }
    hpL.reset(); hpR.reset();
    envWL = envWR = envRL = envRR = envN = 0.0f;
    yL1 = yR1 = 0.0f;
    xfading = false; xfade = 1.0f;
    lastTone = -1.0f;
}

void BbdDelay::reset() noexcept
{
    lineL.clear(); lineR.clear();
    resetSmallState();
    wow.phase = 0.0; flutter.phase = 0.37;
}

void BbdDelay::process (const SpaceContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    const ParamSet& p = *ctx.p;
    float target = p.b (BbdSync) ? (float) syncedMs (p.i (BbdDiv), ctx.bpm) : p[BbdTime];
    target = clampf (target, 30.0f, 2000.0f);
    const float fb = p[BbdFeedback] / 100.0f;
    const float age = p[BbdAge] / 100.0f;
    const float motion = clampf (p[BbdMotion] / 100.0f + ctx.motionMod, 0.0f, 1.0f);
    const float rate = p[BbdRate];
    const int stereo = p.i (BbdStereo);
    const bool tape = p.i (BbdTimeMode) == 1;
    const float noiseAmt = p[BbdNoise] / 100.0f;

    const float tone = p[BbdTone];
    if (tone != lastTone || age != lastAge)
    {
        lastTone = tone; lastAge = age;
        const double cut = tone * (1.0 - 0.45 * age);
        for (int k = 0; k < 2; ++k) { fbLpL[k].lowpass (sr, cut, k == 0 ? 0.54 : 1.31); fbLpR[k].lowpass (sr, cut, k == 0 ? 0.54 : 1.31); }
        const double inCut = std::min (18000.0, tone * 1.8);
        inLpL.lowpass (sr, inCut); inLpR.lowpass (sr, inCut);
        hpL.setCutoff (sr, 45.0); hpR.setCutoff (sr, 45.0);
    }
    wow.setRate (rate, sr);
    flutter.setRate (rate * 6.3 + 3.0, sr);
    const float drive = 1.0f + age * 2.5f;
    const float compAmt = age * 0.5f;

    // time handling
    if (tape)
    {
        xfading = false;
        const float c = onePoleCoef (0.25f, sr);
        (void) c;
    }
    else if (! xfading && std::abs (target - headA) > 0.5f)
    {
        headB = target; xfading = true; xfade = 0.0f;
        xfadeStep = 1.0f / (0.035f * (float) sr);
    }
    const float tapeCoef = onePoleCoef (0.25f, sr);
    const float spreadMs = stereo == 2 ? 9.0f : 0.0f;
    const int maxD = lineL.maxDelay() - 8;

    for (int i = 0; i < n; ++i)
    {
        if (tape) { headA += tapeCoef * (target - headA); }
        const float w = wow.next(), fl = flutter.next(), dr = drift.next();
        const float modMs = motion * (2.2f * w + 0.12f * fl + 0.7f * dr) * std::min (1.0f, headA / 120.0f + 0.25f);
        const float modMsR = stereo == 2 ? motion * (2.2f * -w + 0.12f * fl + 0.7f * dr) : modMs;
        auto readAt = [&] (const DelayLine& d, float ms) {
            const float ds = clampf (ms * 0.001f * (float) sr, 2.0f, (float) maxD);
            return d.readCubic (ds);
        };
        float rl, rr;
        if (xfading)
        {
            const float a = 1.0f - xfade, b = xfade;
            rl = a * readAt (lineL, headA + modMs) + b * readAt (lineL, headB + modMs);
            rr = a * readAt (lineR, headA + spreadMs + modMsR) + b * readAt (lineR, headB + spreadMs + modMsR);
            xfade += xfadeStep;
            if (xfade >= 1.0f) { xfading = false; headA = headB; xfade = 1.0f; }
        }
        else
        {
            rl = readAt (lineL, headA + modMs);
            rr = readAt (lineR, headA + spreadMs + modMsR);
        }
        // expander (companding colour): read-side envelope
        envRL += (std::abs (rl) > envRL ? envA : envRelSlow) * (std::abs (rl) - envRL);
        envRR += (std::abs (rr) > envRR ? envA : envRelSlow) * (std::abs (rr) - envRR);
        if (compAmt > 0.0f)
        {
            rl *= clampf (std::pow ((envRL + 0.02f) / 0.27f, compAmt * 0.6f), 0.4f, 2.2f);
            rr *= clampf (std::pow ((envRR + 0.02f) / 0.27f, compAmt * 0.6f), 0.4f, 2.2f);
        }
        // loop/reconstruction filtering
        float yl = rl, yr = rr;
        for (int k = 0; k < 2; ++k) { yl = fbLpL[k].process (yl); yr = fbLpR[k].process (yr); }
        outL[i] = yl;
        outR[i] = yr;

        float fl1 = hpL.process (yl), fr1 = hpR.process (yr);
        fl1 = fastTanh (fl1 * drive) / drive * fb;
        fr1 = fastTanh (fr1 * drive) / drive * fb;

        float xl = inL[i], xr = inR[i];
        if (stereo != 0) { const float m = 0.5f * (xl + xr); xl = m; xr = m; }
        xl = inLpL.process (xl);
        xr = inLpR.process (xr);
        float wl, wr;
        if (stereo == 1) { wl = xl + fr1; wr = fl1; }          // ping-pong: input enters left only
        else { wl = xl + fl1; wr = xr + fr1; }
        // compressor (write side)
        envWL += (std::abs (wl) > envWL ? envA : envRel) * (std::abs (wl) - envWL);
        envWR += (std::abs (wr) > envWR ? envA : envRel) * (std::abs (wr) - envWR);
        if (compAmt > 0.0f)
        {
            wl *= clampf (std::pow ((envWL + 0.02f) / 0.27f, -compAmt * 0.5f), 0.45f, 2.5f);
            wr *= clampf (std::pow ((envWR + 0.02f) / 0.27f, -compAmt * 0.5f), 0.45f, 2.5f);
        }
        wl = fastTanh (wl * (1.0f + age * 0.6f)) / (1.0f + age * 0.6f);
        wr = fastTanh (wr * (1.0f + age * 0.6f)) / (1.0f + age * 0.6f);
        if (noiseAmt > 0.0f)
        {
            const float e = 0.5f * (envWL + envWR);
            envN += 0.001f * (e - envN);
            wl += noiseRng.bi() * envN * noiseAmt * 0.08f;
            wr += noiseRng.bi() * envN * noiseAmt * 0.08f;
        }
        lineL.push (wl);
        lineR.push (wr);
    }
    timeMs = headA;
}

// ============================================================================ Interval delay
void IntervalDelay::prepare (double sampleRate)
{
    sr = sampleRate;
    maxDelaySamples = (int) std::ceil (7.2 * sr);
    mainL.allocate (maxDelaySamples + 64);
    mainR.allocate (maxDelaySamples + 64);
    clears.count = 0;
    clears.add (mainL); clears.add (mainR);
    for (auto& t : taps)
    {
        t.aaL.allocate (maxDelaySamples + 64);
        t.aaR.allocate (maxDelaySamples + 64);
        clears.add (t.aaL); clears.add (t.aaR);
    }
    for (int k = 0; k < 2; ++k)
    {
        fbApL[k].allocate ((int) (0.02 * sr)); fbApR[k].allocate ((int) (0.02 * sr));
        outApL[k].allocate ((int) (0.02 * sr)); outApR[k].allocate ((int) (0.02 * sr));
    }
    reset();
}

void IntervalDelay::resetSmallState() noexcept
{
    fbLpL.reset(); fbLpR.reset(); fbHpL.reset(); fbHpR.reset();
    for (int k = 0; k < 2; ++k) { fbApL[k].clear(); fbApR[k].clear(); outApL[k].clear(); outApR[k].clear(); }
    for (int k = 0; k < kTaps; ++k)
    {
        auto& t = taps[k];
        for (int s = 0; s < 2; ++s) { t.aaFiltL[s].reset(); t.aaFiltR[s].reset(); }
        t.phase = 0.17 * k; t.revU = 0.31 * k; t.idW = 1.0f;
        t.frag[0] = Grain(); t.frag[1] = Grain(); t.fragNext = 0;
    }
}

void IntervalDelay::reset() noexcept
{
    mainL.clear(); mainR.clear();
    for (auto& t : taps) { t.aaL.clear(); t.aaR.clear(); }
    resetSmallState();
    written = 0;
}

float IntervalDelay::reverseAcquisitionMs (const ParamSet& p) const noexcept
{
    return p[IvGrain] * 2.0f;
}

float IntervalDelay::readTap (const Tap& t, bool left, double delay) const noexcept
{
    const float d = (float) std::clamp (delay, 2.0, (double) maxDelaySamples);
    return left ? t.aaL.readCubic (d) : t.aaR.readCubic (d);
}

void IntervalDelay::process (const SpaceContext& ctx, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    const ParamSet& p = *ctx.p;
    float target = p.b (IvSync) ? (float) syncedMs (p.i (IvDiv), ctx.bpm) : p[IvTime];
    target = clampf (target, 20.0f, 4000.0f);
    const int numTaps = std::clamp (p.i (IvTaps), 1, kTaps);
    const bool clock = p.i (IvPitchMode) == 1;
    const bool cascade = p.i (IvShiftPlace) == 1;
    const int dirMode = p.i (IvDirection);
    const float smear = p[IvSmear] / 100.0f;
    const double W = std::max (32.0, p[IvGrain] * 0.001 * sr);
    float fb = p[IvFeedback] / 100.0f;
    if (cascade) fb = std::min (fb, 0.9f) * 0.85f;
    fbLpL.lowpass (sr, p[IvTone]); fbLpR.lowpass (sr, p[IvTone]);
    fbHpL.setCutoff (sr, 80.0); fbHpR.setCutoff (sr, 80.0);
    for (int k = 0; k < 2; ++k)
    {
        const float g = smear * 0.55f;
        fbApL[k].g = g; fbApR[k].g = g; outApL[k].g = g * 0.9f; outApR[k].g = g * 0.9f;
    }
    const float apD1 = (float) (0.0071 * sr), apD2 = (float) (0.0113 * sr), apD3 = (float) (0.0053 * sr), apD4 = (float) (0.0089 * sr);

    for (int k = 0; k < kTaps; ++k)
    {
        auto& t = taps[k];
        const float semis = (float) p.i (IvTap1Semi + k) + ctx.pitchDriverSemis;
        t.ratioTarget = std::exp2 (clampf (semis, -36.0f, 36.0f) / 12.0f);
        t.level = k < numTaps ? dbToGain (p[IvTap1Level + k]) : 0.0f;
        panGains (p[IvTap1Pan + k] / 100.0f, t.gl, t.gr);
        // anti-alias the copy for upward rates
        const float cut = t.ratioTarget > 1.0f ? (float) (0.45 * sr / t.ratioTarget) : (float) (0.47 * sr);
        if (std::abs (cut - t.aaCut) > 1.0f)
        {
            t.aaCut = cut;
            for (int s = 0; s < 2; ++s)
            {
                t.aaFiltL[s].lowpass (sr, cut, s == 0 ? 0.54 : 1.31);
                t.aaFiltR[s].lowpass (sr, cut, s == 0 ? 0.54 : 1.31);
            }
        }
    }
    const float ratioCoef = onePoleCoef (0.03f, sr);
    const float timeCoef = onePoleCoef (0.12f, sr);
    const float idCoef = onePoleCoef (0.02f, sr);

    for (int i = 0; i < n; ++i)
    {
        timeSmoothed += timeCoef * (target - timeSmoothed);
        const double T = timeSmoothed * 0.001 * sr;
        float sumL = 0.0f, sumR = 0.0f, casL = 0.0f, casR = 0.0f;
        for (int k = 0; k < numTaps; ++k)
        {
            auto& t = taps[k];
            t.ratio += ratioCoef * (t.ratioTarget - t.ratio);
            const bool rev = dirMode == 1 || (dirMode == 2 && (k & 1));
            float yl = 0.0f, yr = 0.0f;
            if (! clock)
            {
                if (! rev)
                {
                    const float idTarget = std::abs (t.ratio - 1.0f) < 5.0e-4f ? 1.0f : 0.0f;
                    t.idW += idCoef * (idTarget - t.idW);
                    const double base = std::max (0.0, T - 2.0 - W * 0.5);
                    if (t.idW < 0.999f)
                    {
                        const double prevPh = t.phase;
                        t.phase += (1.0 - t.ratio) / W;
                        t.phase -= std::floor (t.phase);
                        if (t.phase < prevPh - 0.5 || t.phase > prevPh + 0.5) t.jit[0] = rng.uni() * smear;
                        double ph2 = t.phase + 0.5; if (ph2 >= 1.0) ph2 -= 1.0;
                        if ((ph2 < 0.01 && t.ratio < 1.0f) || (ph2 > 0.99 && t.ratio > 1.0f)) t.jit[1] = rng.uni() * smear;
                        const double d1 = base + 2.0 + t.phase * W + t.jit[0] * W * 0.25;
                        const double d2 = base + 2.0 + ph2 * W + t.jit[1] * W * 0.25;
                        const float s1 = std::sin (kPi * (float) t.phase);
                        const float w1 = s1 * s1, w2 = 1.0f - w1;
                        yl = w1 * readTap (t, true, d1) + w2 * readTap (t, true, d2);
                        yr = w1 * readTap (t, false, d1) + w2 * readTap (t, false, d2);
                    }
                    if (t.idW > 0.001f)
                    {
                        const double d = base + 2.0 + W * 0.5;
                        yl = t.idW * readTap (t, true, d) + (1.0f - t.idW) * yl;
                        yr = t.idW * readTap (t, false, d) + (1.0f - t.idW) * yr;
                    }
                }
                else
                {
                    // reverse grains: read backwards through past windows at speed 'ratio'
                    t.revU += 1.0 / W;
                    if (t.revU >= 1.0) { t.revU -= 1.0; t.jit[0] = rng.uni() * smear; }
                    double u2 = t.revU + 0.5; if (u2 >= 1.0) u2 -= 1.0;
                    const double span = (1.0 + t.ratio) * W;
                    const double d1 = T + 2.0 + t.revU * span + t.jit[0] * W * 0.2;
                    const double d2 = T + 2.0 + u2 * span + t.jit[0] * W * 0.2;
                    const float s1 = std::sin (kPi * (float) t.revU);
                    const float w1 = s1 * s1, w2 = 1.0f - w1;
                    yl = w1 * readTap (t, true, d1) + w2 * readTap (t, true, d2);
                    yr = w1 * readTap (t, false, d1) + w2 * readTap (t, false, d2);
                }
            }
            else
            {
                // Clock: fragments read at 'ratio' speed; pitch and duration are coupled.
                const double lseg = std::min (1.2 * sr, W * 2.0 * (1.0 + smear));
                for (int g = 0; g < 2; ++g)
                {
                    auto& f = t.frag[g];
                    if (! f.active) continue;
                    const double pos = f.start + (double) f.dir * f.rate * f.t;
                    const double delay = (double) written - pos;
                    float win = 1.0f;
                    if (f.t < f.fade) { const float s = std::sin (0.5f * kPi * (float) (f.t / f.fade)); win = s * s; }
                    else if (f.t > f.dur - f.fade) { const float s = std::sin (0.5f * kPi * (float) ((f.dur - f.t) / f.fade)); win = s * s; }
                    yl += win * readTap (t, true, delay);
                    yr += win * readTap (t, false, delay);
                    f.t += 1.0;
                    if (f.t >= f.dur) f.active = false;
                }
                // schedule next fragment when the current one enters its fade-out (or none is running)
                auto& last = t.frag[(t.fragNext + 1) & 1];
                if (! last.active || last.t >= last.dur - last.fade)
                {
                    auto& f = t.frag[t.fragNext];
                    if (! f.active)
                    {
                        f.active = true; f.t = 0.0;
                        f.rate = t.ratio;
                        f.dir = rev ? -1 : 1;
                        f.dur = lseg / f.rate;
                        f.fade = std::max (16.0, f.dur * 0.2);
                        const double jitter = rng.uni() * smear * lseg * 0.5;
                        f.start = rev ? (double) written - T - 2.0 - jitter : (double) written - T - lseg - jitter;
                        t.fragNext = (t.fragNext + 1) & 1;
                    }
                }
            }
            sumL += yl * t.level * t.gl;
            sumR += yr * t.level * t.gr;
            casL += yl * t.level;
            casR += yr * t.level;
        }
        // output diffusion (smear)
        float oL = sumL, oR = sumR;
        if (smear > 0.01f)
        {
            oL = outApL[1].process (outApL[0].process (oL, apD3), apD4);
            oR = outApR[1].process (outApR[0].process (oR, apD4), apD3);
            oL = lerp (sumL, oL, smear); oR = lerp (sumR, oR, smear);
        }
        outL[i] = oL;
        outR[i] = oR;

        // feedback
        float fsl, fsr;
        if (cascade)
        {
            const float norm = 1.0f / std::sqrt ((float) numTaps);
            fsl = casL * norm; fsr = casR * norm;
        }
        else
        {
            const float d = (float) std::clamp (T, 2.0, (double) maxDelaySamples);
            fsl = mainL.readCubic (d); fsr = mainR.readCubic (d);
        }
        fsl = fbLpL.process (fbHpL.process (fsl));
        fsr = fbLpR.process (fbHpR.process (fsr));
        if (smear > 0.01f)
        {
            fsl = fbApL[1].process (fbApL[0].process (fsl, apD1), apD2);
            fsr = fbApR[1].process (fbApR[0].process (fsr, apD2), apD1);
        }
        fsl = fastTanh (fsl * 0.9f) / 0.9f * fb;
        fsr = fastTanh (fsr * 0.9f) / 0.9f * fb;
        const float wl = inL[i] + fsl, wr = inR[i] + fsr;
        mainL.push (wl);
        mainR.push (wr);
        for (int k = 0; k < kTaps; ++k)
        {
            auto& t = taps[k];
            float al = wl, ar = wr;
            if (t.ratioTarget > 1.0f)
                for (int s = 0; s < 2; ++s) { al = t.aaFiltL[s].process (al); ar = t.aaFiltR[s].process (ar); }
            t.aaL.push (al);
            t.aaR.push (ar);
        }
        ++written;
    }
    timeMs = timeSmoothed;
}

// ============================================================================ Capture looper
void CaptureLooper::prepare (double sampleRate, double maxSeconds)
{
    sr = sampleRate;
    const int s = (int) std::ceil (maxSeconds * sr) + 64;
    bufL.allocate (s);
    bufR.allocate (s);
    clears.count = 0;
    clears.add (bufL); clears.add (bufR);
    reset();
}

void CaptureLooper::reset() noexcept
{
    bufL.clear(); bufR.clear();
    recorded = 0; frozen = false; mix = 0.0f; mixStep = 0.0f; pos = 0;
}

void CaptureLooper::process (bool freeze, float loopSeconds, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    const float fadeStep = 1.0f / (0.06f * (float) sr);
    if (freeze && ! frozen)
    {
        const int maxAvail = (int) std::min<long long> (recorded, (long long) bufL.maxDelay() - 8);
        int want = (int) (loopSeconds * sr);
        int x = std::min ((int) (0.08 * sr), want / 4);
        if (want + x > maxAvail) { want = (int) (maxAvail * 0.8); x = std::min (x, want / 4); }
        if (want > (int) (0.05 * sr))
        {
            frozen = true;
            loopLen = want; xf = std::max (1, x); pos = 0;
            regionStart = loopLen + xf;
        }
    }
    for (int i = 0; i < n; ++i)
    {
        if (frozen)
        {
            mix = freeze ? std::min (1.0f, mix + fadeStep) : std::max (0.0f, mix - fadeStep);
            const int idx = xf + pos;
            float l = bufL.readInt (regionStart - idx), r = bufR.readInt (regionStart - idx);
            if (pos >= loopLen - xf)
            {
                const int j = pos - (loopLen - xf);
                const float t = (float) j / (float) xf;
                const float a = std::cos (t * kPi * 0.5f), b = std::sin (t * kPi * 0.5f);
                l = a * l + b * bufL.readInt (regionStart - j);
                r = a * r + b * bufR.readInt (regionStart - j);
            }
            if (++pos >= loopLen) pos = 0;
            outL[i] = (1.0f - mix) * inL[i] + mix * l;
            outR[i] = (1.0f - mix) * inR[i] + mix * r;
            if (! freeze && mix <= 0.0f) { frozen = false; recorded = 0; }
        }
        else
        {
            bufL.push (inL[i]);
            bufR.push (inR[i]);
            ++recorded;
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
    }
}
} // namespace pa
