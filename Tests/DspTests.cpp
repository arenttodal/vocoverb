// Method- and engine-level DSP tests (harmony methods, delays, reverbs, freeze stability).
#include "TestUtil.h"

#include "DSP/Delay/Delays.h"
#include "DSP/Harmony/HarmonyUnit.h"
#include "DSP/Reverb/Reverbs.h"

using namespace pa;
using namespace pat;

namespace
{
/** Runs one HarmonyUnit with a fixed note set over a stereo input. */
Buf runMethod (int method, const Buf& in, std::vector<std::pair<double, std::vector<int>>> noteChanges, ParamSet p = ParamSet(), double sr = 48000.0)
{
    VoiceBank vb; vb.prepare (sr);
    CarrierSynth car; car.prepare (sr);
    auto hu = std::make_unique<HarmonyUnit>();
    hu->prepare (sr, 9000);
    p[Transition] = 30; p[NoteAttack] = 5; p[NoteRelease] = 200;
    Buf out; out.L.assign (in.size(), 0.0f); out.R.assign (in.size(), 0.0f);
    float cb[64], cg[64];
    size_t nextChange = 0;
    for (size_t pos = 0; pos < in.size(); pos += 64)
    {
        const double t = (double) pos / sr;
        while (nextChange < noteChanges.size() && noteChanges[nextChange].first <= t)
        {
            NoteSet d; uint32_t o = 1;
            for (int nn : noteChanges[nextChange].second) d.add (nn, 100, o++);
            vb.setDesired (d, true, p);
            ++nextChange;
        }
        const int n = (int) std::min ((size_t) 64, in.size() - pos);
        vb.render (n, p, 0.0f);
        car.render (vb, p, n, cb, cg);
        HarmonyContext ctx; ctx.p = &p; ctx.voices = &vb; ctx.carrier = cb; ctx.carrierGain = cg; ctx.quality = p.i (Quality); ctx.sr = sr;
        hu->process (ctx, method, false, 0, in.L.data() + pos, in.R.data() + pos, out.L.data() + pos, out.R.data() + pos, n, 1.0f, 1.0f);
        vb.clearRetuned();
    }
    return out;
}
} // namespace

TEST ("fft: WOLA identity reconstruction and measured latency == N + N/4")
{
    for (int q = 0; q < 3; ++q)
    {
        const double sr = 48000.0;
        FftVocoder f; f.prepare (sr, 8192); f.identityMode = true;
        VoiceBank vb; vb.prepare (sr);
        ParamSet p; p[Quality] = (float) q;
        Buf in = noiseBurst (1.0, 1.0, sr, 0.3f, 7);
        Buf out = silence (1.0, sr);
        float cb[64] {}, cg[64] {};
        HarmonyContext ctx; ctx.p = &p; ctx.voices = &vb; ctx.carrier = cb; ctx.carrierGain = cg; ctx.quality = q; ctx.sr = sr;
        for (size_t pos = 0; pos < in.size(); pos += 64)
            f.process (ctx, in.L.data() + pos, in.R.data() + pos, out.L.data() + pos, out.R.data() + pos, 64);
        const int N = f.currentSize();
        const int L = f.latencySamples();
        CHECK (L == N + N / 4);
        // find best alignment by search around the declared latency
        int bestLag = -1; double bestErr = 1e9;
        for (int lag = L - 8; lag <= L + 8; ++lag)
        {
            double e = 0, s = 0;
            for (size_t i = (size_t) N * 3; i < in.size(); ++i) { const double d = out.L[i] - in.L[i - (size_t) lag]; e += d * d; s += (double) in.L[i - (size_t) lag] * in.L[i - (size_t) lag]; }
            const double rel = std::sqrt (e / s);
            if (rel < bestErr) { bestErr = rel; bestLag = lag; }
        }
        metric ("fft.N", N); metric ("fft.measuredLatency", bestLag, "samples"); metric ("fft.reconstructionError", db (bestErr), "dB");
        CHECK_MSG (bestLag == L, "measured latency must equal the declared N + N/4");
        CHECK_MSG (db (bestErr) < -80.0, "WOLA reconstruction error too high");
    }
}

TEST ("classic: broadband modulator takes the played chord's harmonics; changing notes changes structure")
{
    Buf in = noiseBurst (4.0, 4.0, 48000.0, 0.2f, 3);
    auto out = runMethod (MethodClassic, in, { { 0.0, { 48, 52, 55 } }, { 2.0, { 50, 54, 57 } } });
    const double inFracA = chordFraction (in.L, 48000.0, 0.5, 1.9, { 48, 52, 55 });
    const double fA = chordFraction (out.L, 48000.0, 0.5, 1.9, { 48, 52, 55 });
    const double fA_B = chordFraction (out.L, 48000.0, 0.5, 1.9, { 50, 54, 57 });
    const double fB = chordFraction (out.L, 48000.0, 2.5, 3.9, { 50, 54, 57 });
    const double fB_A = chordFraction (out.L, 48000.0, 2.5, 3.9, { 48, 52, 55 });
    metric ("classic.noiseBaseline", inFracA); metric ("classic.fracA", fA); metric ("classic.fracA_onB", fA_B);
    metric ("classic.fracB", fB); metric ("classic.fracB_onA", fB_A);
    CHECK (fA > 0.6 && fA > inFracA * 1.5);
    CHECK (fB > 0.6 && fB > fB_A * 1.5);
    const double level = db (segRms (out.L, 48000.0, 0.5, 1.9) / segRms (in.L, 48000.0, 0.5, 1.9));
    metric ("classic.levelVsInput", level, "dB");
    CHECK (level > -12.0 && level < 6.0);
}

TEST ("fft: carrier vocoding follows notes and differs from Classic")
{
    Buf in = noiseBurst (4.0, 4.0, 48000.0, 0.2f, 3);
    auto out = runMethod (MethodFft, in, { { 0.0, { 48, 52, 55 } }, { 2.0, { 50, 54, 57 } } });
    const double fA = chordFraction (out.L, 48000.0, 0.6, 1.9, { 48, 52, 55 });
    const double fB = chordFraction (out.L, 48000.0, 2.6, 3.9, { 50, 54, 57 });
    const double fB_A = chordFraction (out.L, 48000.0, 2.6, 3.9, { 48, 52, 55 });
    metric ("fft.fracA", fA); metric ("fft.fracB", fB); metric ("fft.fracB_onA", fB_A);
    CHECK (fA > 0.6 && fB > 0.6 && fB > fB_A * 1.5);
    auto cl = runMethod (MethodClassic, in, { { 0.0, { 48, 52, 55 } } });
    auto ff = runMethod (MethodFft, in, { { 0.0, { 48, 52, 55 } } });
    // compare after aligning FFT latency (N + N/4 = 2560)
    double num = 0, da = 0, dbb = 0;
    for (size_t i = 48000; i < 150000; ++i) { const double a = cl.L[i], b = ff.L[i + 2560]; num += a * b; da += a * a; dbb += b * b; }
    const double corr = num / std::sqrt (da * dbb + 1e-30);
    metric ("classicVsFft.correlation", corr);
    CHECK_MSG (corr < 0.9, "FFT must not be a reskinned Classic");
    const double level = db (segRms (out.L, 48000.0, 0.6, 1.9) / segRms (in.L, 48000.0, 0.6, 1.9));
    metric ("fft.levelVsInput", level, "dB");
    CHECK (level > -12.0 && level < 6.0);
}

TEST ("resonator: broadband excitation rings at selected notes; note-off decays; retune bounded")
{
    Buf in = noiseBurst (4.0, 6.0, 48000.0, 0.2f, 5);
    ParamSet p; p[RsDecay] = 1.0f;
    auto out = runMethod (MethodResonator, in, { { 0.0, { 48, 55 } }, { 2.0, { 50, 57 } }, { 4.0, {} } }, p);
    const double fA = chordFraction (out.L, 48000.0, 0.5, 1.9, { 48, 55 }, 25.0);
    const double fB = chordFraction (out.L, 48000.0, 2.5, 3.9, { 50, 57 }, 25.0);
    metric ("res.fracA", fA); metric ("res.fracB", fB);
    CHECK (fA > 0.7 && fB > 0.7);
    // tuning accuracy: dominant peak near C3 harmonics region
    const double f0 = dominantFrequency (out.L.data() + 24000, 48000, 48000.0, 100.0, 200.0);
    metric ("res.peakNearC3", f0, "Hz");
    CHECK (std::abs (1200.0 * std::log2 (f0 / 130.8128)) < 15.0);
    // decay after note-off and input end (4 s): 1 s RT60 => down by > 40 dB within ~1.5 s
    const double tail = segRms (out.L, 48000.0, 5.6, 6.0), body = segRms (out.L, 48000.0, 3.0, 3.9);
    metric ("res.decayDrop", db (tail / body), "dB");
    CHECK (db (tail / body) < -40.0);
    CHECK (finite (out) && segPeak (out.L, 48000.0, 0.0, 6.0) < 2.0);
}

TEST ("shift: known sine moves by the relative interval; zero shift is identity (pure delay)")
{
    const double sr = 48000.0;
    Buf in = sine (440.0, 2.0, sr, 0.3f);
    ParamSet p; p[ShRef] = 48; p[Colour] = 100;
    auto up7 = runMethod (MethodShift, in, { { 0.0, { 55 } } }, p);
    const double f = dominantFrequency (up7.L.data() + 24000, 65536, sr, 300.0, 1200.0);
    metric ("shift.+7.freq", f, "Hz");
    CHECK (std::abs (1200.0 * std::log2 (f / (440.0 * std::exp2 (7.0 / 12.0)))) < 10.0);
    auto dn12 = runMethod (MethodShift, in, { { 0.0, { 36 } } }, p);
    const double f2 = dominantFrequency (dn12.L.data() + 24000, 65536, sr, 100.0, 600.0);
    metric ("shift.-12.freq", f2, "Hz");
    CHECK (std::abs (1200.0 * std::log2 (f2 / 220.0)) < 30.0); // granular comb lines at the grain rate limit peak precision
    auto zero = runMethod (MethodShift, in, { { 0.0, { 48 } } }, p);
    IntervalShift tmp; tmp.prepare (sr);
    const int L = tmp.latencyForQuality (1);
    metric ("shift.latency", L, "samples");
    // after the voice attack, identity path == input delayed by L times the voice gain (constant)
    double num = 0, den = 0;
    for (size_t i = 30000; i < 90000; ++i) { num += (double) zero.L[i] * in.L[i - (size_t) L]; den += (double) in.L[i - (size_t) L] * in.L[i - (size_t) L]; }
    const double g = num / den;
    double err = 0, ref = 0;
    for (size_t i = 30000; i < 90000; ++i) { const double d = zero.L[i] - g * in.L[i - (size_t) L]; err += d * d; ref += g * g * in.L[i - (size_t) L] * in.L[i - (size_t) L]; }
    metric ("shift.identityError", db (std::sqrt (err / ref)), "dB");
    CHECK (db (std::sqrt (err / ref)) < -60.0);
}

TEST ("bbd: max feedback stays bounded; zero input + noise param stays silent")
{
    const double sr = 48000.0;
    BbdDelay d; d.prepare (sr);
    ParamSet p; p[BbdFeedback] = 95; p[BbdAge] = 100; p[BbdMotion] = 100; p[BbdTime] = 60; p[BbdNoise] = 100;
    SpaceContext c; c.p = &p;
    Buf in = noiseBurst (0.5, 30.0, sr, 0.5f);
    Buf out = silence (30.0, sr);
    for (size_t pos = 0; pos < in.size(); pos += 64)
        d.process (c, in.L.data() + pos, in.R.data() + pos, out.L.data() + pos, out.R.data() + pos, 64);
    const double pk = segPeak (out.L, sr, 0.0, 30.0);
    metric ("bbd.maxFeedbackPeak", pk);
    CHECK (finite (out) && pk < 2.0);
    BbdDelay d2; d2.prepare (sr);
    Buf z = silence (3.0, sr), o = silence (3.0, sr);
    for (size_t pos = 0; pos < z.size(); pos += 64) d2.process (c, z.L.data() + pos, z.R.data() + pos, o.L.data() + pos, o.R.data() + pos, 64);
    CHECK (segPeak (o.L, sr, 0.0, 3.0) < 1e-7);
}

TEST ("interval: stable vs clock differ; cascade bounded; reverse produces output; never reads the future")
{
    const double sr = 48000.0;
    auto run = [&] (ParamSet p, const Buf& in) {
        auto d = std::make_unique<IntervalDelay>(); d->prepare (sr);
        SpaceContext c; c.p = &p;
        Buf out = silence ((double) in.size() / sr, sr);
        for (size_t pos = 0; pos < in.size(); pos += 64)
            d->process (c, in.L.data() + pos, in.R.data() + pos, out.L.data() + pos, out.R.data() + pos, (int) std::min ((size_t) 64, in.size() - pos));
        return out;
    };
    Buf in = sine (330.0, 1.0, sr, 0.3f);
    in.L.resize ((size_t) (8 * sr), 0.0f); in.R.resize (in.L.size(), 0.0f);
    ParamSet ps; ps[IvTime] = 400; ps[IvFeedback] = 40; ps[IvTaps] = 1; ps[IvTap1Semi] = 12; ps[IvTap1Pan] = 0;
    auto stable = run (ps, in);
    // nothing may appear before the echo time (no future reads, no direct signal)
    CHECK (segPeak (stable.L, sr, 0.0, 0.3) < 1e-6);
    const double fS = dominantFrequency (stable.L.data() + (size_t) (0.6 * sr), 16384, sr, 400.0, 1000.0);
    metric ("interval.stable.freq", fS, "Hz");
    CHECK (std::abs (1200.0 * std::log2 (fS / 660.0)) < 15.0);
    ParamSet pc = ps; pc[IvPitchMode] = 1;
    auto clock = run (pc, in);
    double diff = 0, ref = 0;
    for (size_t i = 0; i < stable.size(); ++i) { const double d = stable.L[i] - clock.L[i]; diff += d * d; ref += (double) stable.L[i] * stable.L[i]; }
    metric ("interval.stableVsClock", db (std::sqrt (diff / (ref + 1e-30))), "dB");
    CHECK (diff > ref * 0.1);
    CHECK (segPeak (clock.L, sr, 0.0, 0.3) < 1e-6);
    ParamSet pcas = ps; pcas[IvShiftPlace] = 1; pcas[IvFeedback] = 90; pcas[IvTap1Semi] = 7;
    Buf noise = noiseBurst (0.5, 40.0, sr, 0.5f);
    auto cas = run (pcas, noise);
    const double pk = segPeak (cas.L, sr, 0.0, 40.0), late = segRms (cas.L, sr, 35.0, 40.0), early = segRms (cas.L, sr, 0.4, 2.0);
    metric ("interval.cascade.peak", pk); metric ("interval.cascade.lateVsEarly", db (late / (early + 1e-30)), "dB");
    CHECK (finite (cas) && pk < 2.0 && late < early);
    ParamSet pr = ps; pr[IvDirection] = 1; pr[IvGrain] = 200;
    auto rev = run (pr, in);
    CHECK (segRms (rev.L, sr, 0.6, 1.6) > 1e-3);
}

TEST ("reverbs: plate and wash differ; freeze holds bounded energy; frozen silence stays silent")
{
    const double sr = 48000.0;
    auto run = [&] (int which, ParamSet p, const Buf& in, double freezeAt) {
        auto pl = std::make_unique<PlateReverb>(); auto wa = std::make_unique<WashReverb>();
        pl->prepare (sr); wa->prepare (sr);
        SpaceContext c; c.p = &p;
        Buf out = silence ((double) in.size() / sr, sr);
        for (size_t pos = 0; pos < in.size(); pos += 64)
        {
            c.freeze = freezeAt >= 0.0 && (double) pos / sr >= freezeAt;
            if (which == 0) pl->process (c, in.L.data() + pos, in.R.data() + pos, out.L.data() + pos, out.R.data() + pos, 64);
            else wa->process (c, in.L.data() + pos, in.R.data() + pos, out.L.data() + pos, out.R.data() + pos, 64);
        }
        return out;
    };
    ParamSet p;
    Buf in = noiseBurst (0.05, 6.0, sr, 0.5f);
    auto plate = run (0, p, in, -1.0), wash = run (1, p, in, -1.0);
    // Wash blooms slower: energy in the first 150 ms relative to its 1-2 s energy is lower than the plate's
    const double plEarly = segRms (plate.L, sr, 0.0, 0.15) / segRms (plate.L, sr, 1.0, 2.0);
    const double waEarly = segRms (wash.L, sr, 0.0, 0.15) / segRms (wash.L, sr, 1.0, 2.0);
    metric ("plate.early/late", db (plEarly), "dB"); metric ("wash.early/late", db (waEarly), "dB");
    CHECK (waEarly < plEarly);
    // measured decay (energy drop 1->4 s) : plate 8 s default should drop ~ (3s/8s*60)=22 dB, wash 24 s ~7.5 dB
    const double plDrop = db (segRms (plate.L, sr, 3.5, 4.0) / segRms (plate.L, sr, 1.0, 1.5));
    const double waDrop = db (segRms (wash.L, sr, 3.5, 4.0) / segRms (wash.L, sr, 1.0, 1.5));
    metric ("plate.drop2.5s", plDrop, "dB"); metric ("wash.drop2.5s", waDrop, "dB");
    CHECK (plDrop < -10.0 && plDrop > -40.0);
    CHECK (waDrop > -12.0 && waDrop < -2.0);
    for (int which = 0; which < 2; ++which)
    {
        Buf burst = noiseBurst (1.0, 60.0, sr, 0.3f, 9);
        auto fr = run (which, p, burst, 1.2);
        const double held = segRms (fr.L, sr, 2.0, 3.0), late = segRms (fr.L, sr, 58.0, 60.0);
        metric (which ? "wash.freeze.60s" : "plate.freeze.60s", db (late / held), "dB");
        CHECK (finite (fr));
        CHECK (late > held * 0.35 && late < held * 1.6);
        Buf z = silence (10.0, sr);
        auto zs = run (which, p, z, 0.5);
        CHECK (segPeak (zs.L, sr, 0.0, 10.0) < 1e-7);
    }
}
