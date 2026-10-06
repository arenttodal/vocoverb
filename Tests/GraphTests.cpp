// Whole-engine acceptance tests: dry integrity, leakage, revoicing, placement, routing, freeze, timing, stability.
#include "TestUtil.h"

#include <chrono>

using namespace pa;
using namespace pat;

namespace
{
constexpr double kSr = 48000.0;
const double kSilence = 1.0e-6; // -120 dBFS
} // namespace

TEST ("graph: dry null - Live is immediate, Studio delayed exactly by declared latency")
{
    Buf in = noiseBurst (2.0, 2.0, kSr, 0.4f, 11);
    for (int timing = 0; timing < 2; ++timing)
        for (int routing = 0; routing < 3; ++routing)
            for (int placement = 0; placement < 2; ++placement)
            {
                ParamSet p; p[Timing] = (float) timing; p[WetLevel] = -60; p[DryLevel] = 0; p[WetOnly] = 0;
                p[Routing] = (float) routing; p[Placement] = (float) placement; p[HarmMethod] = 2;
                Engine probe; probe.prepare (kSr, 256);
                const int L = timing == 1 ? probe.studioLatency (1) : 0;
                auto out = render (p, in, {}, kSr, 256);
                double maxErr = 0;
                for (size_t i = (size_t) L; i < in.size(); ++i)
                    maxErr = std::max ({ maxErr, (double) std::abs (out.L[i] - in.L[i - (size_t) L]), (double) std::abs (out.R[i] - in.R[i - (size_t) L]) });
                double pre = 0; for (int i = 0; i < L; ++i) pre = std::max (pre, (double) std::abs (out.L[(size_t) i]));
                if (routing == 0 && placement == 0) { metric (timing ? "dryNull.studio.maxErr" : "dryNull.live.maxErr", maxErr); metric ("dryNull.latency", L, "samples"); }
                CHECK_MSG (maxErr == 0.0, "dry path must be bit-exact (timing " + std::to_string (timing) + ")");
                CHECK (pre == 0.0);
            }
}

TEST ("graph: wet-only impulse - no direct dry leak; creative pre-delay remains")
{
    for (int method = 0; method < 5; ++method)
    {
        ParamSet p = baseParams (method);
        p[PlPredelay] = 50; p[DelayEnable] = 0;
        Buf in = silence (1.0, kSr); in.L[0] = in.R[0] = 1.0f;
        auto out = render (p, in, chordEvents (0.0, 0.9, { 48, 55 }));
        const double early = segPeak (out.L, kSr, 0.0, 0.040);
        metric ("wetOnlyImpulse.earlyPeak.m" + std::to_string (method), early);
        CHECK (early < kSilence);
    }
    ParamSet p = baseParams (0); p[DelayEnable] = 1; p[ReverbEnable] = 0; p[BbdTime] = 300;
    Buf in = silence (1.0, kSr); in.L[0] = in.R[0] = 1.0f;
    auto out = render (p, in);
    CHECK (segPeak (out.L, kSr, 0.0, 0.25) < kSilence);
    CHECK (segPeak (out.L, kSr, 0.28, 0.45) > 1e-3);
}

TEST ("graph: no carrier leakage - MIDI with silent input is numerically silent (all methods/placements/routings)")
{
    Buf in = silence (2.0, kSr);
    auto ev = chordEvents (0.0, 1.5, { 48, 52, 55, 59, 62, 65 });
    double worst = 0;
    for (int method = 1; method < 5; ++method)
        for (int placement = 0; placement < 2; ++placement)
            for (int routing = 0; routing < 3; ++routing)
            {
                ParamSet p = baseParams (method);
                p[DelayEnable] = 1; p[Placement] = (float) placement; p[Routing] = (float) routing; p[BbdNoise] = 100; p[BbdAge] = 100;
                p[WetOnly] = 0; p[DryLevel] = 0;
                auto out = render (p, in, ev);
                worst = std::max (worst, segPeak (out.L, kSr, 0.0, 2.0));
                worst = std::max (worst, segPeak (out.R, kSr, 0.0, 2.0));
                CHECK (finite (out));
            }
    metric ("leakage.worstPeak", db (worst), "dBFS");
    CHECK (worst < kSilence);
}

TEST ("graph: zero Depth equals ordinary ambience; full Depth with no notes removes ordinary wet")
{
    Buf in = noiseBurst (1.0, 3.0, kSr, 0.3f, 4);
    for (int method = 1; method < 5; ++method)
    {
        ParamSet off = baseParams (0);
        ParamSet zero = baseParams (method); zero[Depth] = 0;
        auto a = render (off, in, chordEvents (0.0, 2.0, { 48, 55 }));
        auto b = render (zero, in, chordEvents (0.0, 2.0, { 48, 55 }));
        // Live timing: the ordinary branch is aligned to the method's buffering latency (FFT N, Shift W/2)
        // Classic-only build: every legacy method value runs Classic (no buffering latency in Live timing)
        HarmonyUnit probe; probe.prepare (kSr, 9000);
        const size_t L = (size_t) probe.methodLatency (MethodClassic, 1);
        double maxD = 0; for (size_t i = L; i < a.size(); ++i) maxD = std::max (maxD, (double) std::abs (a.L[i - L] - b.L[i]));
        CHECK_MSG (maxD < 1e-6, "Depth 0 must equal Off for method " + std::to_string (method));
        ParamSet full = baseParams (method); full[NoNotePolicy] = 1; // Release, and no MIDI at all
        full[NoteSource] = 0;
        auto c = render (full, in, { { 0.0, kSeqStatus, SeqPanic, 0 } }); // panic: no notes at all (stored chord fallback disabled)
        const double resid = segPeak (c.L, kSr, 0.0, 3.0);
        metric ("fullDepthNoNotes.peak.m" + std::to_string (method), resid);
        CHECK (resid < kSilence);
    }
}

TEST ("graph: tail revoicing (After Space) - chords change the surviving tail without new audio")
{
    for (int method : { 1, 2 })
    {
        ParamSet p = baseParams (method);
        p[ReverbMode] = 1; p[WaDecay] = 30; p[WaBloom] = 20; p[Transition] = 50;
        Buf in = noiseBurst (0.6, 7.0, kSr, 0.4f, 21);
        auto ev = chordEvents (0.0, 3.5, { 48, 52, 55 });
        append (ev, chordEvents (3.5, 7.0, { 54, 58, 61 }));
        auto out = render (p, in, ev);
        const double aOnA = chordFraction (out.L, kSr, 1.8, 3.3, { 48, 52, 55 }), bOnA = chordFraction (out.L, kSr, 1.8, 3.3, { 54, 58, 61 });
        const double bOnB = chordFraction (out.L, kSr, 4.5, 6.5, { 54, 58, 61 }), aOnB = chordFraction (out.L, kSr, 4.5, 6.5, { 48, 52, 55 });
        const std::string m = method == 1 ? "classic" : "fft";
        metric ("revoice." + m + ".chordA_during_A", aOnA); metric ("revoice." + m + ".chordB_during_A", bOnA);
        metric ("revoice." + m + ".chordB_during_B", bOnB); metric ("revoice." + m + ".chordA_during_B", aOnB);
        metric ("revoice." + m + ".tailLevel", db (segRms (out.L, kSr, 4.5, 6.5)), "dBFS");
        CHECK (aOnA > 0.7 && aOnA > bOnA * 2.0);
        CHECK (bOnB > 0.7 && bOnB > aOnB * 2.0);
        CHECK (segRms (out.L, kSr, 4.5, 6.5) > 1e-4);
    }
}

TEST ("graph: Before Space keeps earlier chords in the generated tail")
{
    ParamSet p = baseParams (1);
    p[ReverbMode] = 1; p[WaDecay] = 30; p[WaBloom] = 10; p[Placement] = 1; p[NoNotePolicy] = 0;
    Buf in = noiseBurst (2.0, 5.0, kSr, 0.4f, 31);
    auto ev = chordEvents (0.0, 1.0, { 48, 52, 55 });
    append (ev, chordEvents (1.0, 5.0, { 54, 58, 61 }));
    auto before = render (p, in, ev);
    const double a = chordFraction (before.L, kSr, 2.6, 4.5, { 48, 52, 55 }), b = chordFraction (before.L, kSr, 2.6, 4.5, { 54, 58, 61 });
    metric ("before.oldChordInTail", a); metric ("before.newChordInTail", b);
    p[Placement] = 0;
    auto after = render (p, in, ev);
    const double a2 = chordFraction (after.L, kSr, 2.6, 4.5, { 48, 52, 55 });
    metric ("after.oldChordInTail", a2);
    CHECK (a > 0.2 && b > 0.2);
    CHECK (a > a2 * 1.5); // After Space retunes the whole tail; Before Space keeps the old chord
}

TEST ("graph: routing relationships and single dry sum")
{
    Buf in = noiseBurst (0.3, 2.5, kSr, 0.3f, 41);
    ParamSet p = baseParams (0); p[DelayEnable] = 1; p[ReverbEnable] = 1; p[BbdTime] = 250;
    // Delay > Reverb with serial send 0 == delay only (reverb hears nothing)
    ParamSet s0 = p; s0[Routing] = 1; s0[SerialSend] = 0;
    ParamSet donly = p; donly[ReverbEnable] = 0;
    auto a = render (s0, in), b = render (donly, in);
    double md = 0; for (size_t i = 0; i < a.size(); ++i) md = std::max (md, (double) std::abs (a.L[i] - b.L[i]));
    metric ("routing.send0_vs_delayOnly", md);
    CHECK (md < 1e-6);
    // Delay > Reverb with full send: reverb appears after the delay time (fed by the delay output)
    ParamSet s1 = p; s1[Routing] = 1; s1[BbdLevel] = -60;
    auto c = render (s1, in);
    CHECK (segPeak (c.L, kSr, 0.0, 0.2) < kSilence);
    CHECK (segRms (c.L, kSr, 0.4, 1.5) > 1e-4);
    // parallel with reverb level -inf == delay only
    ParamSet par = p; par[PlLevel] = -60;
    auto d = render (par, in);
    md = 0; for (size_t i = 0; i < d.size(); ++i) md = std::max (md, (double) std::abs (d.L[i] - b.L[i]));
    CHECK (md < 1e-6);
    // both engines disabled => dry only
    ParamSet none = p; none[DelayEnable] = 0; none[ReverbEnable] = 0; none[WetOnly] = 0; none[DryLevel] = 0;
    auto e = render (none, in);
    md = 0; for (size_t i = 0; i < e.size(); ++i) md = std::max (md, (double) std::abs (e.L[i] - in.L[i]));
    CHECK (md < 1e-6);
    // Reverb > Delay: delay hears the reverb (no output before the delay time), only one dry sum
    ParamSet rd = p; rd[Routing] = 2; rd[PlLevel] = -60; rd[WetOnly] = 0; rd[DryLevel] = 0;
    auto f = render (rd, in);
    double mx = 0; for (size_t i = 0; i < (size_t) (0.2 * kSr); ++i) mx = std::max (mx, (double) std::abs (f.L[i] - in.L[i]));
    CHECK (mx < 1e-6); // exactly one dry copy, no wet before the delay time
    CHECK (segRms (f.L, kSr, 0.4, 2.0) > 1e-4);
}

TEST ("graph: freeze sustains bounded energy, allows After Space chord changes; silent capture stays silent")
{
    for (int target = 0; target < 3; ++target)
    {
        ParamSet p = baseParams (1);
        p[DelayEnable] = 1; p[ReverbMode] = 1; p[WaDecay] = 8; p[FreezeTarget] = (float) target; p[BbdFeedback] = 30;
        Buf in = noiseBurst (1.0, 14.0, kSr, 0.3f, 51);
        auto ev = chordEvents (0.0, 7.0, { 48, 52, 55 });
        append (ev, chordEvents (7.0, 14.0, { 54, 58, 61 }));
        auto out = render (p, in, ev, kSr, 256, [] (double t, ParamSet& q) { q[Freeze] = t >= 1.3 ? 1.0f : 0.0f; });
        const double held = segRms (out.L, kSr, 2.0, 3.0), late = segRms (out.L, kSr, 12.5, 14.0);
        metric ("freeze.t" + std::to_string (target) + ".lateVsHeld", db (late / held), "dB");
        CHECK (finite (out));
        CHECK (late > held * 0.25 && late < held * 2.5);
        const double bOnB = chordFraction (out.L, kSr, 9.0, 13.5, { 54, 58, 61 }), aOnB = chordFraction (out.L, kSr, 9.0, 13.5, { 48, 52, 55 });
        metric ("freeze.t" + std::to_string (target) + ".revoiced", bOnB);
        CHECK (bOnB > 0.6 && bOnB > aOnB * 2.0);
        Buf z = silence (4.0, kSr);
        auto zs = render (p, z, ev, kSr, 256, [] (double, ParamSet& q) { q[Freeze] = 1; });
        CHECK (segPeak (zs.L, kSr, 0.0, 4.0) < kSilence);
    }
}

TEST ("graph: no-note policies (Hold Last / Release / Ambient)")
{
    Buf in = noiseBurst (6.0, 6.0, kSr, 0.25f, 61);
    auto ev = chordEvents (0.0, 2.0, { 48, 52, 55 });
    ParamSet p = baseParams (1); p[PlDecay] = 2; p[NoteRelease] = 300;
    p[NoNotePolicy] = 0;
    auto hold = render (p, in, ev);
    CHECK (chordFraction (hold.L, kSr, 3.5, 5.5, { 48, 52, 55 }) > 0.7);
    p[NoNotePolicy] = 1;
    auto rel = render (p, in, ev);
    metric ("policy.release.after", db (segRms (rel.L, kSr, 3.5, 5.5)), "dBFS");
    CHECK (segRms (rel.L, kSr, 3.5, 5.5) < 1e-5);
    p[NoNotePolicy] = 2;
    auto amb = render (p, in, ev);
    ParamSet off = baseParams (0); off[PlDecay] = 2;
    auto ord = render (off, in, ev);
    const double diff = db (segRms (amb.L, kSr, 4.0, 5.5) / segRms (ord.L, kSr, 4.0, 5.5));
    metric ("policy.ambient.vsOrdinary", diff, "dB");
    CHECK (std::abs (diff) < 0.5);
    // fresh preset, no MIDI received: Hold Last uses the stored chord
    ParamSet st = baseParams (1); st[ChRoot] = 2; st[ChQuality] = 0; // D major stored
    auto fresh = render (st, in, {});
    CHECK (chordFraction (fresh.L, kSr, 2.0, 5.0, { 50, 54, 57 }) > 0.7);
}

TEST ("graph: block-size independence (32..1024) within tolerance; sample-accurate events")
{
    Buf in = noiseBurst (1.5, 3.0, kSr, 0.3f, 71);
    ParamSet p = baseParams (1); p[DelayEnable] = 1; p[ReverbMode] = 1;
    auto ev = chordEvents (0.1234, 1.777, { 48, 55, 62 });
    append (ev, chordEvents (1.9, 2.8, { 50, 57 }));
    auto ref = render (p, in, ev, kSr, 64);
    for (int b : { 32, 128, 512, 1024, 333 })
    {
        auto o = render (p, in, ev, kSr, b);
        double e = 0, r = 0;
        for (size_t i = 0; i < o.size(); ++i) { const double d = o.L[i] - ref.L[i]; e += d * d; r += (double) ref.L[i] * ref.L[i]; }
        const double rel = db (std::sqrt (e / (r + 1e-30)));
        metric ("blocks." + std::to_string (b) + ".relDiff", rel, "dB");
        CHECK (rel < -50.0);
    }
}

TEST ("graph: stereo - mono input, width extremes, mono fold-down, one-sided input")
{
    Buf in = noiseBurst (1.0, 3.0, kSr, 0.3f, 81);
    for (size_t i = 0; i < in.size(); ++i) in.R[i] = in.L[i];
    for (float w : { 0.0f, 100.0f, 150.0f })
    {
        ParamSet p = baseParams (1); p[Width] = w; p[DelayEnable] = 1; p[ReverbMode] = 1;
        auto o = render (p, in, chordEvents (0.0, 3.0, { 48, 55 }));
        CHECK (finite (o));
        double lr = 0, mono = 0, l2 = 0;
        for (size_t i = 0; i < o.size(); ++i) { const double m = 0.5 * (o.L[i] + o.R[i]); mono += m * m; l2 += 0.5 * ((double) o.L[i] * o.L[i] + (double) o.R[i] * o.R[i]); lr += std::abs (o.L[i] - o.R[i]); }
        metric ("stereo.w" + std::to_string ((int) w) + ".monoFold", db (std::sqrt (mono / (l2 + 1e-30))), "dB");
        if (w == 0.0f) CHECK (lr < 1e-3);
        CHECK (db (std::sqrt (mono / (l2 + 1e-30))) > -6.0);
    }
    Buf left = in; for (auto& v : left.R) v = 0.0f;
    ParamSet p = baseParams (0); p[ReverbMode] = 1;
    auto o = render (p, left);
    CHECK (segRms (o.R, kSr, 0.5, 2.5) > segRms (o.L, kSr, 0.5, 2.5) * 0.1);
}

TEST ("graph: stability stress - extremes, automation sweeps, freeze toggles, 60 s")
{
    const auto t0 = std::chrono::steady_clock::now();
    Buf in = noiseBurst (60.0, 60.0, kSr, 0.5f, 91);
    for (size_t i = 0; i < in.size(); ++i) { const double t = (double) i / kSr; if (std::fmod (t, 10.0) > 3.0) { in.L[i] = 0; in.R[i] = 0; } }
    ParamSet p = baseParams (1);
    p[DelayEnable] = 1; p[BbdFeedback] = 95; p[IvFeedback] = 90; p[IvShiftPlace] = 1; p[IvTap1Semi] = 24; p[IvTap2Semi] = -24;
    p[WaDecay] = 120; p[PlDecay] = 30; p[WaMotion] = 100; p[PlMotion] = 100; p[BbdMotion] = 100; p[ShLimit] = 24;
    p[Routing] = 1; p[WetOnly] = 0; p[DryLevel] = 0;
    auto ev = chordEvents (0.0, 30.0, { 24, 36, 96, 108, 60, 72 });
    append (ev, chordEvents (30.0, 60.0, { 30, 100 }));
    auto out = render (p, in, ev, kSr, 512, [] (double t, ParamSet& q) {
        const int k = (int) (t / 2.5);
        q[HarmMethod] = (float) (1 + k % 4);
        q[DelayMode] = (float) (k % 3); q[ReverbMode] = (float) ((k / 2) % 3);
        q[Shimmer] = (k % 4) == 1 ? 100.0f : 0.0f; q[ShimmerPitch] = (float) (k % 5);
        q[TpFeedback] = 110.0f; q[TpHeads] = (float) (k % 7); q[TpDrive] = (float) (k % 2) * 100.0f;
        q[TpTime] = 40.0f + 960.0f * (float) (0.5 + 0.5 * std::sin (t * 0.8));
        q[HaSize] = 5.0f + 95.0f * (float) (0.5 + 0.5 * std::sin (t * 0.6)); q[HaDecay] = 20.0f;
        q[PlSize] = 50.0f + 100.0f * (float) (0.5 + 0.5 * std::sin (t * 1.3));
        q[WaSize] = 25.0f + 175.0f * (float) (0.5 + 0.5 * std::sin (t * 0.7));
        q[BbdTime] = 30.0f + 1970.0f * (float) (0.5 + 0.5 * std::sin (t * 0.9));
        q[IvTime] = 100.0f + 3900.0f * (float) (0.5 + 0.5 * std::sin (t * 0.5));
        q[Freeze] = std::fmod (t, 7.0) > 4.0 ? 1.0f : 0.0f;
        q[FreezeTarget] = (float) (k % 3);
        q[Routing] = (float) (k % 3); q[Placement] = (float) ((k / 3) % 2);
        q[Quality] = (float) ((k / 5) % 3);
    });
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    metric ("stress.audioSeconds", 60.0); metric ("stress.processSeconds", secs, "s");
    CHECK (finite (out));
    const double pk = std::max (segPeak (out.L, kSr, 0.0, 60.0), segPeak (out.R, kSr, 0.0, 60.0));
    metric ("stress.peak", pk);
    CHECK (pk < 4.0);
}

TEST ("graph: smoke matrix at 44.1/48/96 kHz - 4 spaces x 5 methods x 2 placements")
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        Buf in = noiseBurst (0.4, 1.2, sr, 0.3f, 101);
        int count = 0;
        for (int space = 0; space < 4; ++space)
            for (int method = 0; method < 5; ++method)
                for (int placement = 0; placement < 2; ++placement)
                {
                    ParamSet p = baseParams (method);
                    p[DelayEnable] = space < 2 ? 1.0f : 0.0f; p[ReverbEnable] = space >= 2 ? 1.0f : 0.0f;
                    p[DelayMode] = (float) (space % 2); p[ReverbMode] = (float) (space % 2);
                    p[Placement] = (float) placement; p[BbdTime] = 120; p[IvTime] = 150;
                    auto o = render (p, in, chordEvents (0.0, 1.2, { 48, 55, 64 }), sr, sr > 90000 ? 1024 : 128);
                    CHECK (finite (o));
                    CHECK_MSG (segRms (o.L, sr, 0.0, 1.2) > 1e-5, "space " + std::to_string (space) + " method " + std::to_string (method));
                    ++count;
                }
        metric ("smoke.renders@" + std::to_string ((int) sr), count);
    }
}

TEST ("graph: mode / method / quality switches are click-free (bounded sample jumps)")
{
    Buf in = sine (220.0, 4.0, kSr, 0.3f);
    ParamSet p = baseParams (1); p[DelayEnable] = 1; p[Depth] = 60; p[NoteSource] = 1;
    auto base = render (p, in);
    auto sw = render (p, in, {}, kSr, 256, [] (double t, ParamSet& q) {
        q[HarmMethod] = t > 1.0 ? 2.0f : 1.0f; // legacy FFT value: maps to Classic, must not glitch
        q[DelayMode] = t > 1.5 ? 1.0f : 0.0f;
        q[ReverbMode] = t > 2.0 ? 1.0f : 0.0f;
        q[Routing] = t > 2.5 ? 1.0f : 0.0f;
        q[Placement] = t > 3.0 ? 1.0f : 0.0f;
    });
    // reference: steady renders of the first and the final configuration (their own signal content)
    ParamSet fin = p; fin[DelayMode] = 1; fin[ReverbMode] = 1; fin[Routing] = 1; fin[Placement] = 1;
    auto steady = render (fin, in);
    auto maxJump = [] (const std::vector<float>& x, size_t a, size_t b) { double m = 0; for (size_t i = a + 1; i < b; ++i) m = std::max (m, (double) std::abs (x[i] - x[i - 1])); return m; };
    const double ref = std::max (maxJump (base.L, (size_t) (0.5 * kSr), (size_t) (3.9 * kSr)), maxJump (steady.L, (size_t) (0.5 * kSr), (size_t) (3.9 * kSr)));
    const double got = maxJump (sw.L, (size_t) (0.5 * kSr), (size_t) (3.9 * kSr));
    metric ("switch.maxJumpRef", ref); metric ("switch.maxJump", got);
    CHECK (got < std::max (ref * 3.0, 0.02));
}

TEST ("graph: Studio latency aligns ordinary and harmonised paths (Depth 0 == Off in Studio)")
{
    Buf in = noiseBurst (1.0, 2.0, kSr, 0.3f, 111);
    for (int method : { 2, 4 })
    {
        ParamSet a = baseParams (0); a[Timing] = 1;
        ParamSet b = baseParams (method); b[Timing] = 1; b[Depth] = 0;
        auto x = render (a, in), y = render (b, in);
        double md = 0; for (size_t i = 0; i < x.size(); ++i) md = std::max (md, (double) std::abs (x.L[i] - y.L[i]));
        CHECK (md < 1e-6);
    }
}

TEST ("graph: real-time path performs no heap allocation (all modes, events, switches, freeze, tail kill)")
{
    auto eng = std::make_unique<Engine>();
    eng->prepare (kSr, 512);
    ParamSet p = baseParams (1);
    p[DelayEnable] = 1;
    Buf in = noiseBurst (1.0, 1.0, kSr, 0.3f, 7);
    std::vector<float> oL (512), oR (512);
    std::vector<MidiEvent> ev = { { 3, 0x90, 48, 100, Origin::Host }, { 10, 0x90, 55, 90, Origin::Keyboard }, { 100, 0xB0, 64, 127, Origin::Host },
                                  { 200, 0xE0, 0, 80, Origin::Host }, { 300, 0x80, 48, 0, Origin::Host }, { 400, 0xC0, 2, 0, Origin::Host } };
    TransportInfo tp; tp.hasHost = true; tp.playing = true; tp.bpm = 120; tp.ppqAtBlockStart = 0;
    // warm-up (first-time state) outside the measured region
    eng->process (p, tp, in.L.data(), in.R.data(), oL.data(), oR.data(), 512, ev.data(), (int) ev.size());
    gAllocCount.store (0);
    gAllocArmed.store (true);
    for (int b = 0; b < 2000; ++b)
    {
        const int k = b / 40;
        p[HarmMethod] = (float) (k % 5); p[DelayMode] = (float) (k % 3); p[ReverbMode] = (float) ((k / 2) % 3);
        p[Shimmer] = (k % 3) == 0 ? 60.0f : 0.0f; p[TpHeads] = (float) (k % 7);
        p[Routing] = (float) ((k / 3) % 3); p[Placement] = (float) ((k / 4) % 2); p[NoteSource] = (float) ((k / 5) % 4);
        p[Freeze] = (k % 7) > 4 ? 1.0f : 0.0f; p[Quality] = (float) ((k / 9) % 3); p[Timing] = (float) ((k / 6) % 2);
        p[IvPitchMode] = (float) ((k / 2) % 2); p[IvDirection] = (float) (k % 3);
        if (b % 300 == 150) eng->requestTailKill();
        if (b % 250 == 100) eng->requestPanic();
        tp.ppqAtBlockStart += 512.0 / kSr * 2.0;
        const size_t off = (size_t) ((b * 512) % (int) (in.size() - 512));
        eng->process (p, tp, in.L.data() + off, in.R.data() + off, oL.data(), oR.data(), 512, (b % 10 == 0) ? ev.data() : nullptr, (b % 10 == 0) ? (int) ev.size() : 0);
    }
    gAllocArmed.store (false);
    metric ("rt.allocations", (double) gAllocCount.load());
    CHECK_MSG (gAllocCount.load() == 0, "allocations on the audio path: " + std::to_string (gAllocCount.load()));
}

TEST ("graph: sample-rate transitions re-prepare safely (48 -> 96 -> 44.1 kHz) with buffers 32/1024")
{
    auto eng = std::make_unique<Engine>();
    ParamSet p = baseParams (2);
    p[DelayEnable] = 1; p[ReverbMode] = 1; p[Quality] = 2; p[Timing] = 1;
    std::vector<MidiEvent> ev = { { 0, 0x90, 48, 100, Origin::Host }, { 0, 0x90, 55, 100, Origin::Host } };
    for (double sr : { 48000.0, 96000.0, 44100.0, 96000.0 })
        for (int block : { 32, 1024 })
        {
            eng->prepare (sr, block);
            Buf in = noiseBurst (0.5, 1.5, sr, 0.3f, 5);
            Buf out = silence (1.5, sr);
            TransportInfo tp;
            for (size_t pos = 0; pos + (size_t) block <= in.size(); pos += (size_t) block)
                eng->process (p, tp, in.L.data() + pos, in.R.data() + pos, out.L.data() + pos, out.R.data() + pos, block, pos == 0 ? ev.data() : nullptr, pos == 0 ? 2 : 0);
            CHECK (finite (out));
            CHECK_MSG (segRms (out.L, sr, 0.6, 1.4) > 1e-5, "sr " + std::to_string ((int) sr) + " block " + std::to_string (block));
            const int L = eng->studioLatency (2);
            metric ("srTransition.studioLatency@" + std::to_string ((int) sr), L, "samples");
        }
}



TEST ("graph: Dry/Wet endpoints - 0% is the dry path only, 100% removes the dry, 50% equals the original blend")
{
    Buf in = noiseBurst (0.5, 2.0, kSr, 0.3f, 21);
    ParamSet p = baseParams (1); p[WetOnly] = 0; p[DryLevel] = 0; p[WetLevel] = -6; p[NoteSource] = 1;
    ParamSet dry0 = p; dry0[Mix] = 0;
    auto a = render (dry0, in);
    double e0 = 0; for (size_t i = 0; i < in.size(); ++i) e0 = std::max (e0, (double) std::abs (a.L[i] - in.L[i]));
    metric ("mix0.maxDiffFromDry", e0);
    CHECK_MSG (e0 < 1e-6, "Dry/Wet 0% must output only the dry signal");
    ParamSet wet100 = p; wet100[Mix] = 100;
    ParamSet wetOnly = p; wetOnly[WetOnly] = 1;
    auto b = render (wet100, in), c = render (wetOnly, in);
    double e1 = 0; for (size_t i = 0; i < in.size(); ++i) e1 = std::max (e1, (double) std::abs (b.L[i] - c.L[i]));
    metric ("mix100.maxDiffFromWetOnly", e1);
    CHECK_MSG (e1 < 1e-6, "Dry/Wet 100% must equal Wet Only");
    ParamSet half = p; half[Mix] = 50;
    ParamSet legacy = p; // default mix = 50 -> identical to the pre-mix build
    auto d = render (half, in), e = render (legacy, in);
    double e2 = 0; for (size_t i = 0; i < in.size(); ++i) e2 = std::max (e2, (double) std::abs (d.L[i] - e.L[i]));
    CHECK (e2 == 0.0);
    // Wet Only overrides the blend but keeps it: wet at full level regardless of the stored mix
    ParamSet wo35 = wetOnly; wo35[Mix] = 35;
    auto f = render (wo35, in);
    double e3 = 0; for (size_t i = 0; i < in.size(); ++i) e3 = std::max (e3, (double) std::abs (f.L[i] - c.L[i]));
    CHECK (e3 < 1e-6);
}
