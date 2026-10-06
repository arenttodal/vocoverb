#include "Engine.h"

namespace pa
{
namespace
{
inline void zeroBuf (float* x, int n) noexcept { std::memset (x, 0, sizeof (float) * (size_t) n); }
constexpr int kClearBudget = 65536;
} // namespace

Engine::Engine()
{
    zeroBuf (zero, kChunk);
}

void Engine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = std::max (1, maxBlockSize);
    voices.prepare (sr);
    arp.prepare (sr);
    carrier.prepare (sr);
    bbd.prepare (sr);
    ivd.prepare (sr);
    delayLooper.prepare (sr, 4.5);
    plate.prepare (sr);
    wash.prepare (sr);
    const int maxLat = 8192 + (int) std::ceil (0.1 * sr);
    hA.prepare (sr, maxLat);
    hB.prepare (sr, maxLat);
    dryDelay.allocate (maxLat + 64);
    const int ramp = std::max (1, (int) (0.02 * sr));
    dryGain.setRampLength (ramp);
    wetGain.setRampLength (ramp);
    wetOnlyGain.setRampLength (ramp);
    telemetry.sampleRate.store ((float) sr);
    reset();
}

void Engine::reset()
{
    notes.reset();
    voices.reset();
    arp.reset();
    carrier.reset();
    bbd.reset(); ivd.reset(); delayLooper.reset(); plate.reset(); wash.reset();
    hA.reset(); hB.reset();
    dryDelay.clear();
    lowCutL.reset(); lowCutR.reset(); highCutL.reset(); highCutR.reset();
    lastLowCut = lastHighCut = -1.0f;
    duckEnv = 0.0f; limEnv = 0.0f;
    depthCur = 0.0f; ambient = 1.0f;
    needsInit = true;
    lastDesired.clear();
    midiEverReceived = false;
    notesDirty = true;
    bendSemis = 0.0f; modWheel = 0.0f; pitchDriver = 0.0f;
    lastArpNote = -1; seqFreeze = false;
    hasPendingArp = false; arpSkipFirstTick = false;
    timeSec = 0.0;
    bucketCount = 0; bucketDelay = bucketReverb = bucketWet = bucketDry = 0.0f;
}

void Engine::initFromParams() noexcept
{
    dryGain.reset (dbToGain (p[DryLevel]) * mixDryGain (p[Mix]));
    wetGain.reset (dbToGain (p[WetLevel]) * dbToGain (p[WetTrim]) * (p.b (WetOnly) ? 1.0f : mixWetGain (p[Mix])));
    wetOnlyGain.reset (p.b (WetOnly) ? 0.0f : 1.0f);
    dip = 1.0f; dipState = 0;
    topoKey = -1; pendingTopoKey = -1;
    appliedQuality = std::clamp (p.i (Quality), 0, 2);
    pendingQuality = -1;
    studio = p.i (Timing) == 1;
    studioLat = studioLatency (appliedQuality);
    killState = 0; killGain = 1.0f;
    dStage = Stage(); rStage = Stage();
    dStage.active = p.i (DelayMode); rStage.active = p.i (ReverbMode);
    dStage.state[dStage.active] = 1; dStage.gain[dStage.active] = 1.0f;
    rStage.state[rStage.active] = 1; rStage.gain[rStage.active] = 1.0f;
    if (! p.b (DelayEnable)) { dStage.gain[dStage.active] = 0.0f; dStage.state[dStage.active] = 0; }
    if (! p.b (ReverbEnable)) { rStage.gain[rStage.active] = 0.0f; rStage.state[rStage.active] = 0; }
    {
        const int m = effectiveHarmonyMethod (p);
        hA.setInitialMethod (m); hB.setInitialMethod (m);
        depthCur = m == MethodOff ? 0.0f : clampf (p[Depth] / 100.0f, 0.0f, 1.0f);
        ambient = 1.0f;
    }
    needsInit = false;
}

int Engine::studioLatency (int quality) const noexcept
{
    return std::max (hA.methodLatency (MethodFft, quality), hA.methodLatency (MethodShift, quality));
}

// ---------------------------------------------------------------------------------------------------- events
void Engine::handleEvent (const MidiEvent& e) noexcept
{
    const double t = timeSec + (double) e.offset / sr;
    if (e.status == kSeqStatus)
    {
        switch (e.d1)
        {
            case SeqFreezeOn: seqFreeze = true; break;
            case SeqFreezeOff: seqFreeze = false; break;
            case SeqPanic:
                notes.allNotesOff(); voices.killAll (15.0f); arp.reset();
                midiEverReceived = true; notesDirty = true; bendSemis = 0.0f;
                break;
            case SeqTailKill: cmdTailKill.store (true); break;
            default: break;
        }
        return;
    }
    const int type = e.status & 0xF0, ch = e.status & 0x0F;
    telemetry.midiCounter.fetch_add (1, std::memory_order_relaxed);
    if (e.origin == Origin::Host)
    {
        telemetry.hostMidiCounter.fetch_add (1, std::memory_order_relaxed);
        telemetry.lastHostChannel.store (ch + 1, std::memory_order_relaxed);
        const int filter = p.i (MidiChannel);
        if (filter != 0 && ch + 1 != filter) { telemetry.hostFilteredCounter.fetch_add (1, std::memory_order_relaxed); return; }
        if (type == 0x90 && e.d2 > 0)
        {
            telemetry.hostNoteOnCounter.fetch_add (1, std::memory_order_relaxed);
            telemetry.lastHostNote.store (e.d1, std::memory_order_relaxed);
        }
    }
    switch (type)
    {
        case 0x90:
            if (e.d2 > 0)
            {
                notes.noteOn (e.origin, ch, e.d1, e.d2, t);
                midiEverReceived = true;
                telemetry.noteEventCounter.fetch_add (1, std::memory_order_relaxed);
                notesDirty = true;
                break;
            }
            [[fallthrough]];
        case 0x80:
            notes.noteOff (e.origin, ch, e.d1, t);
            notesDirty = true;
            break;
        case 0xB0:
            if (e.d1 == 64) { notes.sustain (ch, e.d2 >= 64, t); notesDirty = true; }
            else if (e.d1 == 1) modWheel = (float) e.d2 / 127.0f;
            else if (e.d1 == 123 || e.d1 == 124 || e.d1 == 125 || e.d1 == 126 || e.d1 == 127)
            {
                // All Notes Off releases that origin's notes (a fresh preset's stored chord is not cancelled by it)
                notes.allNotesOff (e.origin); notesDirty = true;
            }
            else if (e.d1 == 120)
            {
                notes.allNotesOff(); voices.killAll (10.0f); midiEverReceived = true; notesDirty = true;
                beginTailKill();
            }
            else if (e.d1 == 121) { bendSemis = 0.0f; modWheel = 0.0f; notes.sustain (ch, false, t); notesDirty = true; }
            break;
        case 0xE0:
        {
            const int v = ((int) e.d2 << 7 | (int) e.d1) - 8192;
            const float range = p.i (PitchBendRange) == 1 ? 12.0f : 2.0f;
            bendSemis = (float) v / 8192.0f * range;
            break;
        }
        case 0xC0:
            if (e.d1 < 8) telemetry.snapshotRequest.store (e.d1);
            break;
        default: break;
    }
}

void Engine::updateNotes (bool force) noexcept
{
    const int src = p.i (NoteSource);
    NoteSet desired;
    bool replacement = false;
    NoteSet chord;
    buildChord (p, chord);
    const bool chordChanged = ! chord.sameNotes (chordSet);
    const NoteSet& midi = notes.effective();
    const bool midiReplace = notes.consumeReplacement();

    switch (src)
    {
        case 1: // stored chord
            desired = chord;
            replacement = chordChanged && lastDesired.n > 0;
            break;
        case 2: // intervals
        {
            int root = -1, vel = 100;
            if (p.i (IntRefSource) == 0) { root = p.i (IntRoot); replacement = true; }
            else
            {
                root = midi.lowest();
                for (int i = 0; i < midi.n; ++i) if (midi.e[(size_t) i].note == root) vel = midi.e[(size_t) i].vel;
                replacement = midiReplace;
            }
            buildIntervals (p, root, vel, desired);
            break;
        }
        case 3: // arp
        {
            const int note = arp.currentNote();
            if (note >= 0) desired.add (note, 100, 1);
            break;
        }
        default: // MIDI
            if (midi.n == 0 && ! midiEverReceived && p.i (NoNotePolicy) == 0) { desired = chord; replacement = chordChanged && lastDesired.n > 0; }
            else { desired = midi; replacement = midiReplace; }
            break;
    }
    chordSet = chord;
    if (force || ! desired.sameNotes (lastDesired) || replacement)
    {
        // velocity for arp notes is carried by the pending arp event
        if (src == 3 && desired.n == 1 && hasPendingArp == false) desired.e[0].vel = (uint8_t) std::clamp (pendingArp.vel, 1, 127);
        voices.setDesired (desired, replacement, p);
        lastDesired = desired;
        int m[4] = { 0, 0, 0, 0 };
        for (int i = 0; i < desired.n; ++i) { const int nn = desired.e[(size_t) i].note; m[nn >> 5] |= (int) (1u << (nn & 31)); }
        telemetry.desiredMask0.store (m[0]); telemetry.desiredMask1.store (m[1]);
        telemetry.desiredMask2.store (m[2]); telemetry.desiredMask3.store (m[3]);
    }
    notesDirty = false;

    // Interval-delay pitch driver
    const int drv = p.i (IvDriver);
    if (drv == 1)
    {
        int lo = src == 3 ? midi.lowest() : desired.lowest();
        if (lo < 0) lo = voices.lowestGatedNote();
        if (lo >= 0) pitchDriver = (float) (lo - p.i (IvRef));
    }
    else if (drv == 2)
    {
        if (lastArpNote >= 0) pitchDriver = (float) (lastArpNote - p.i (IvRef));
    }
    else pitchDriver = 0.0f;
}


// ---------------------------------------------------------------------------------------------------- stages
namespace
{
/** Shared two-mode stage logic: the wanted mode receives input; the other rings out (zero input), fades, then
    clears progressively. Re-activating a clearing mode finishes its clear synchronously (bounded). */
template <typename Run, typename BeginClear, typename ClearStep, typename Stage>
void runStage (Stage& s, int want, bool enabled, bool fastFade, double sr, const float* levelsDb,
               const float* inL, const float* inR, const float* zero, float* tmpL, float* tmpR,
               float* rawL, float* rawR, float* levL, float* levR, int n,
               Run run, BeginClear beginClear, ClearStep clearStep)
{
    for (int i = 0; i < n; ++i) { rawL[i] = rawR[i] = levL[i] = levR[i] = 0.0f; }
    const float upStep = (float) n / (0.03f * (float) sr);
    for (int m = 0; m < 2; ++m)
    {
        const bool wanted = enabled && m == want;
        if (wanted)
        {
            if (s.state[m] == 3) { while (! clearStep (m, 1 << 20)) {} s.state[m] = 0; }
            s.state[m] = 1;
        }
        else if (s.state[m] == 1) s.state[m] = 2;

        const float g0 = s.gain[m];
        if (s.state[m] == 1) s.gain[m] = std::min (1.0f, g0 + upStep);
        else if (s.state[m] == 2)
        {
            const float fadeSec = ! enabled ? 0.05f : (fastFade ? 0.03f : 1.5f);
            s.gain[m] = std::max (0.0f, g0 - (float) n / (fadeSec * (float) sr));
        }
        const float g1 = s.gain[m];
        const float lv0 = s.level[m], lv1 = dbToGain (levelsDb[m]);
        s.level[m] = lv1;
        if (s.state[m] == 1 || s.state[m] == 2)
        {
            run (m, s.state[m] == 1 ? inL : zero, s.state[m] == 1 ? inR : zero, tmpL, tmpR, n);
            const float gs = (g1 - g0) / (float) n, ls = (lv1 - lv0) / (float) n;
            for (int i = 0; i < n; ++i)
            {
                const float g = g0 + gs * (float) i, lv = lv0 + ls * (float) i;
                rawL[i] += tmpL[i] * g; rawR[i] += tmpR[i] * g;
                levL[i] += tmpL[i] * g * lv; levR[i] += tmpR[i] * g * lv;
            }
            if (s.state[m] == 2 && g1 <= 0.0f) { s.state[m] = 3; beginClear (m); }
        }
        else if (s.state[m] == 3)
        {
            if (clearStep (m, kClearBudget)) s.state[m] = 0;
        }
    }
}
} // namespace

float Engine::runDelay (const SpaceContext& sc, bool freeze, const float* inL, const float* inR,
                        float* rawL, float* rawR, float* levL, float* levR, int n) noexcept
{
    // freeze: new input is cancelled (bounded overdub) and the recent output is looped
    const float target = freeze ? sc.overdub : 1.0f;
    const float g0 = dStage.inputGain;
    const float step = (float) n / (0.05f * (float) sr);
    dStage.inputGain = g0 < target ? std::min (target, g0 + step) : std::max (target, g0 - step);
    const float gs = (dStage.inputGain - g0) / (float) n;
    for (int i = 0; i < n; ++i) { const float g = g0 + gs * (float) i; inScL[i] = inL[i] * g; inScR[i] = inR[i] * g; }

    const float levels[2] = { p[BbdLevel], p[IvLevel] };
    runStage (dStage, p.i (DelayMode), p.b (DelayEnable), p.b (ClearTailOnChange), sr, levels, inScL, inScR, zero, tmpL, tmpR,
              rawL, rawR, levL, levR, n,
              [&] (int m, const float* a, const float* b, float* o1, float* o2, int len) {
                  if (m == 0) bbd.process (sc, a, b, o1, o2, len); else ivd.process (sc, a, b, o1, o2, len);
              },
              [&] (int m) { if (m == 0) bbd.beginClear(); else ivd.beginClear(); },
              [&] (int m, int budget) { return m == 0 ? bbd.clearStep (budget) : ivd.clearStep (budget); });

    if (killState != 2) delayLooper.clearStep (kClearBudget / 4); // finishes any stale-capture clear
    // loop capture on the delay output (records continuously while not frozen)
    const float tMs = dStage.active == 0 ? bbd.currentTimeMs() : ivd.currentTimeMs();
    float loopSec = tMs * 0.001f * std::max (1.0f, std::round (1.0f / std::max (0.03f, tMs * 0.001f)));
    loopSec = clampf (loopSec, 0.25f, 3.5f);
    // apply looper to raw, then rebuild levelled output with the current level mix
    float lvl = 0.0f, gsum = 0.0f;
    for (int m = 0; m < 2; ++m) { lvl += dStage.gain[m] * dStage.level[m]; gsum += dStage.gain[m]; }
    const float levelMix = gsum > 1.0e-6f ? lvl / gsum : dbToGain (dStage.active == 0 ? p[BbdLevel] : p[IvLevel]);
    if (freeze || delayLooper.isFrozen())
    {
        delayLooper.process (freeze, loopSec, rawL, rawR, rawL, rawR, n);
        for (int i = 0; i < n; ++i) { levL[i] = rawL[i] * levelMix; levR[i] = rawR[i] * levelMix; }
    }
    else delayLooper.process (false, loopSec, rawL, rawR, tmpL, tmpR, n); // record only
    telemetry.delayFrozen.store (delayLooper.isFrozen(), std::memory_order_relaxed);
    telemetry.delayTimeMs.store (tMs, std::memory_order_relaxed);
    return dStage.enable();
}

float Engine::runReverb (const SpaceContext& sc, const float* inL, const float* inR,
                         float* rawL, float* rawR, float* levL, float* levR, int n) noexcept
{
    const float levels[2] = { p[PlLevel], p[WaLevel] };
    runStage (rStage, p.i (ReverbMode), p.b (ReverbEnable), p.b (ClearTailOnChange), sr, levels, inL, inR, zero, tmpL, tmpR,
              rawL, rawR, levL, levR, n,
              [&] (int m, const float* a, const float* b, float* o1, float* o2, int len) {
                  if (m == 0) plate.process (sc, a, b, o1, o2, len); else wash.process (sc, a, b, o1, o2, len);
              },
              [&] (int m) { if (m == 0) plate.beginClear(); else wash.beginClear(); },
              [&] (int m, int budget) { return m == 0 ? plate.clearStep (budget) : wash.clearStep (budget); });
    telemetry.reverbFrozen.store (sc.freeze, std::memory_order_relaxed);
    return rStage.enable();
}

void Engine::beginTailKill() noexcept
{
    if (killState == 1 || killState == 2) return;
    killState = 1;
}

bool Engine::clearProgress (int budget) noexcept
{
    bool done = true;
    done = bbd.clearStep (budget) && done;
    done = ivd.clearStep (budget) && done;
    done = delayLooper.clearStep (budget) && done;
    done = plate.clearStep (budget) && done;
    done = wash.clearStep (budget) && done;
    return done;
}

// ---------------------------------------------------------------------------------------------------- process
void Engine::process (const ParamSet& params, const TransportInfo& tp, const float* inL, const float* inR,
                      float* outL, float* outR, int numSamples, const MidiEvent* events, int numEvents) noexcept
{
    ScopedFlushDenormals noDenormals;
    p = params;
    if (needsInit) initFromParams();
    if (tp.playing != transport.playing) arp.reset();
    transport = tp;

    if (cmdStateReset.exchange (false))
    {
        notes.allNotesOff(); voices.killAll (40.0f); arp.reset();
        midiEverReceived = false; seqFreeze = false; notesDirty = true;
        if (! delayLooper.isFrozen()) delayLooper.beginClear(); // drop stale captured audio
    }
    if (cmdPanic.exchange (false))
    {
        notes.allNotesOff(); voices.killAll (15.0f); arp.reset();
        midiEverReceived = true; notesDirty = true;
        bendSemis = 0.0f;
    }
    if (cmdTailKill.exchange (false))
    {
        notes.allNotesOff(); voices.killAll (15.0f); arp.reset();
        midiEverReceived = true; notesDirty = true; seqFreeze = false;
        beginTailKill();
    }

    notes.setPolicy (p.i (NoNotePolicy));
    notes.setLatch (p.b (Latch));
    notes.setChordWindow (p[ChordWindow] / 1000.0);

    // quality changes clear the wet state through the tail-kill path (no allocation)
    const int q = std::clamp (p.i (Quality), 0, 2);
    if (q != appliedQuality) { pendingQuality = q; beginTailKill(); }

    // topology / timing changes use a short wet (and dry for timing) dip
    const int key = p.i (Routing) * 1000 + p.i (Placement) * 100 + p.i (ApplyHarmonyTo) * 10 + p.i (Timing);
    if (topoKey < 0)
    {
        topoKey = key; activeRouting = p.i (Routing); activePlacement = p.i (Placement); activeApply = p.i (ApplyHarmonyTo);
        studio = p.i (Timing) == 1; studioLat = studioLatency (appliedQuality);
    }
    else if (key != topoKey && dipState == 0) { pendingTopoKey = key; dipState = 1; }

    const double bpm = (tp.hasHost && tp.bpm > 1.0) ? tp.bpm : (double) p[Tempo];
    const bool hostClock = tp.hasHost && tp.playing && tp.ppqAtBlockStart >= 0.0;
    const double ppqPerSample = bpm / 60.0 / sr;
    const bool arpOn = p.i (NoteSource) == 3;

    int pos = 0, e = 0;
    while (pos < numSamples)
    {
        while (e < numEvents && events[e].offset <= pos) handleEvent (events[e++]);
        if (hasPendingArp) { hasPendingArp = false; }
        int end = std::min (numSamples, pos + kChunk);
        if (e < numEvents && events[e].offset < end) end = std::max (pos + 1, events[e].offset);

        if (arpOn)
        {
            NoteSet pool = notes.effective();
            if (pool.n == 0) pool = chordSet.n ? chordSet : pool;
            if (pool.n == 0) { NoteSet c; buildChord (p, c); pool = c; }
            for (int j = pos; j < end; ++j)
            {
                if (arpSkipFirstTick && j == pos) { arpSkipFirstTick = false; continue; }
                const double ppqNow = hostClock ? tp.ppqAtBlockStart + ppqPerSample * j : -1.0;
                const auto o = arp.tick (p, pool, ppqNow, bpm);
                if (o.changed)
                {
                    if (o.gate) { lastArpNote = o.note; pendingArp = o; telemetry.arpNote.store (o.note); }
                    else telemetry.arpNote.store (-1);
                    if (j == pos) { notesDirty = true; updateNotes (true); }
                    else { end = j; arpSkipFirstTick = true; notesDirty = true; break; }
                }
            }
        }
        else arpSkipFirstTick = false;

        processChunk (inL + pos, inR + pos, outL + pos, outR + pos, end - pos);
        timeSec += (double) (end - pos) / sr;
        pos = end;
        if (arpSkipFirstTick) updateNotes (true);
    }
    while (e < numEvents) handleEvent (events[e++]);

    for (int v = 0; v < kMaxVoices; ++v)
    {
        const auto& s = voices.slot (v);
        telemetry.voiceNote[(size_t) v].store (s.note, std::memory_order_relaxed);
        telemetry.voiceEnv[(size_t) v].store (s.env, std::memory_order_relaxed);
        telemetry.voiceGate[(size_t) v].store (s.gate, std::memory_order_relaxed);
    }
    telemetry.holdingLast.store (notes.isHolding(), std::memory_order_relaxed);
    telemetry.activeMethod.store (hA.activeMethod(), std::memory_order_relaxed);
    telemetry.dryLatency.store (studio ? studioLat : 0, std::memory_order_relaxed);
    telemetry.wetLatency.store (hA.currentLatency(), std::memory_order_relaxed);
    telemetry.clearing.store (killState, std::memory_order_relaxed);
}

void Engine::processChunk (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    updateNotes (notesDirty);
    voices.render (n, p, bendSemis);
    carrier.render (voices, p, n, carrierBuf, carrierGain);

    HarmonyContext ctx;
    ctx.tap = &voiceTap;
    ctx.p = &p; ctx.voices = &voices; ctx.carrier = carrierBuf; ctx.carrierGain = carrierGain;
    ctx.quality = appliedQuality; ctx.sr = sr;

    // depth (with mod wheel and the Ambient no-note policy)
    const int method = effectiveHarmonyMethod (p); // Classic-only build (legacy methods map to Classic)
    const int modTarget = p.i (ModWheelTarget);
    float dT = method == MethodOff ? 0.0f : clampf (p[Depth] / 100.0f + (modTarget == 0 ? modWheel : 0.0f), 0.0f, 1.0f);
    if (p.i (NoNotePolicy) == 2 && p.i (NoteSource) != 3)
    {
        const bool empty = lastDesired.n == 0;
        const float tSec = empty ? std::max (0.05f, p[NoteRelease] / 1000.0f) : std::max (0.02f, p[NoteAttack] / 1000.0f);
        const float step = (float) n / (tSec * (float) sr);
        ambient = empty ? std::max (0.0f, ambient - step) : std::min (1.0f, ambient + step);
    }
    else ambient = std::min (1.0f, ambient + (float) n / (0.05f * (float) sr));
    dT *= ambient;
    const float dStart = depthCur;
    const float dEnd = depthCur + (dT - depthCur) * std::min (1.0f, (float) n / (0.03f * (float) sr));
    depthCur = dEnd;

    const bool freeze = p.b (Freeze) || seqFreeze;
    const int fTarget = p.i (FreezeTarget);
    SpaceContext sc;
    sc.p = &p;
    sc.bpm = (transport.hasHost && transport.bpm > 1.0) ? transport.bpm : (double) p[Tempo];
    sc.pitchDriverSemis = pitchDriver;
    sc.freeze = freeze && (fTarget == 0 || fTarget == 2);
    sc.overdub = p[FreezeOverdub] / 100.0f * 0.5f;
    sc.motionMod = modTarget == 1 ? modWheel * 0.5f : 0.0f;
    sc.quality = appliedQuality;
    const bool freezeDelay = freeze && fTarget >= 1;

    // ---- tail kill / clearing state machine
    const float killStep = (float) n / (0.03f * (float) sr);
    if (killState == 1)
    {
        killGain = std::max (0.0f, killGain - killStep);
        if (killGain <= 0.0f)
        {
            killState = 2;
            bbd.beginClear(); ivd.beginClear(); delayLooper.beginClear(); plate.beginClear(); wash.beginClear();
            hA.reset(); hB.reset();
            if (pendingQuality >= 0) { appliedQuality = pendingQuality; pendingQuality = -1; studioLat = studioLatency (appliedQuality); }
        }
    }
    else if (killState == 3)
    {
        killGain = std::min (1.0f, killGain + killStep);
        if (killGain >= 1.0f) killState = 0;
    }

    // ---- topology dip (fades the wet output AND the signal entering the spaces, so a swap never
    //      records a discontinuity into a delay/reverb tail that would replay later)
    const float dipBefore = dip;
    if (dipState == 1)
    {
        dip = std::max (0.0f, dip - (float) n / (0.025f * (float) sr));
        if (dip <= 0.0f && pendingTopoKey >= 0)
        {
            topoKey = pendingTopoKey; pendingTopoKey = -1;
            activeRouting = p.i (Routing); activePlacement = p.i (Placement); activeApply = p.i (ApplyHarmonyTo);
            studio = p.i (Timing) == 1; studioLat = studioLatency (appliedQuality);
            hA.reset(); hB.reset(); // the harmony units now see a different signal: start from clean envelopes
            dipState = 2;
        }
    }
    else if (dipState == 2)
    {
        dip = std::min (1.0f, dip + (float) n / (0.025f * (float) sr));
        if (dip >= 1.0f) dipState = 0;
    }

    const float* gInL = inL; const float* gInR = inR;
    if (dipState != 0 || dipBefore < 1.0f)
    {
        const float dStep = (dip - dipBefore) / (float) std::max (1, n);
        for (int i = 0; i < n; ++i) { const float gD = dipBefore + dStep * (float) (i + 1); dipInL[i] = inL[i] * gD; dipInR[i] = inR[i] * gD; }
        gInL = dipInL; gInR = dipInR;
    }

    float* wl = wetL; float* wr = wetR;
    if (killState == 2)
    {
        zeroBuf (wl, n); zeroBuf (wr, n);
        if (clearProgress (kClearBudget)) killState = 3;
    }
    else
    {
        const int routing = activeRouting, placement = activePlacement, apply = activeApply;
        const float send = p[SerialSend] / 100.0f;
        if (placement == 0) // After Space
        {
            if (routing == 0)
            {
                runDelay (sc, freezeDelay, gInL, gInR, sEL, sER, sAL, sAR, n);
                runReverb (sc, gInL, gInR, sFL, sFR, sBL, sBR, n);
                hA.process (ctx, method, studio, studioLat, sAL, sAR, sCL, sCR, n, apply != 2 ? dStart : 0.0f, apply != 2 ? dEnd : 0.0f);
                hB.process (ctx, method, studio, studioLat, sBL, sBR, sDL, sDR, n, apply != 1 ? dStart : 0.0f, apply != 1 ? dEnd : 0.0f);
                for (int i = 0; i < n; ++i) { wl[i] = sCL[i] + sDL[i]; wr[i] = sCR[i] + sDR[i]; }
            }
            else if (routing == 1)
            {
                const float eD = runDelay (sc, freezeDelay, gInL, gInR, sEL, sER, sAL, sAR, n);
                for (int i = 0; i < n; ++i) { sCL[i] = eD * sEL[i] * send + (1.0f - eD) * gInL[i]; sCR[i] = eD * sER[i] * send + (1.0f - eD) * gInR[i]; }
                runReverb (sc, sCL, sCR, sFL, sFR, sBL, sBR, n);
                for (int i = 0; i < n; ++i) { sDL[i] = sAL[i] + sBL[i]; sDR[i] = sAR[i] + sBR[i]; }
                hA.process (ctx, method, studio, studioLat, sDL, sDR, wl, wr, n, dStart, dEnd);
            }
            else
            {
                const float eR = runReverb (sc, gInL, gInR, sFL, sFR, sBL, sBR, n);
                for (int i = 0; i < n; ++i) { sCL[i] = eR * sFL[i] * send + (1.0f - eR) * gInL[i]; sCR[i] = eR * sFR[i] * send + (1.0f - eR) * gInR[i]; }
                runDelay (sc, freezeDelay, sCL, sCR, sEL, sER, sAL, sAR, n);
                for (int i = 0; i < n; ++i) { sDL[i] = sAL[i] + sBL[i]; sDR[i] = sAR[i] + sBR[i]; }
                hA.process (ctx, method, studio, studioLat, sDL, sDR, wl, wr, n, dStart, dEnd);
            }
        }
        else // Before Space: harmonise the input once, then the space
        {
            hA.process (ctx, method, studio, studioLat, gInL, gInR, sCL, sCR, n, dStart, dEnd);  // u
            hB.process (ctx, MethodOff, studio, studioLat, gInL, gInR, sDL, sDR, n, 0.0f, 0.0f); // aligned x
            if (routing == 0)
            {
                const float* dInL = apply != 2 ? sCL : sDL; const float* dInR = apply != 2 ? sCR : sDR;
                const float* rInL = apply != 1 ? sCL : sDL; const float* rInR = apply != 1 ? sCR : sDR;
                runDelay (sc, freezeDelay, dInL, dInR, sEL, sER, sAL, sAR, n);
                runReverb (sc, rInL, rInR, sFL, sFR, sBL, sBR, n);
                for (int i = 0; i < n; ++i) { wl[i] = sAL[i] + sBL[i]; wr[i] = sAR[i] + sBR[i]; }
            }
            else if (routing == 1)
            {
                const float eD = runDelay (sc, freezeDelay, sCL, sCR, sEL, sER, sAL, sAR, n);
                for (int i = 0; i < n; ++i) { sDL[i] = eD * sEL[i] * send + (1.0f - eD) * sCL[i]; sDR[i] = eD * sER[i] * send + (1.0f - eD) * sCR[i]; }
                runReverb (sc, sDL, sDR, sFL, sFR, sBL, sBR, n);
                for (int i = 0; i < n; ++i) { wl[i] = sAL[i] + sBL[i]; wr[i] = sAR[i] + sBR[i]; }
            }
            else
            {
                const float eR = runReverb (sc, sCL, sCR, sFL, sFR, sBL, sBR, n);
                for (int i = 0; i < n; ++i) { sDL[i] = eR * sFL[i] * send + (1.0f - eR) * sCL[i]; sDR[i] = eR * sFR[i] * send + (1.0f - eR) * sCR[i]; }
                runDelay (sc, freezeDelay, sDL, sDR, sEL, sER, sAL, sAR, n);
                for (int i = 0; i < n; ++i) { wl[i] = sAL[i] + sBL[i]; wr[i] = sAR[i] + sBR[i]; }
            }
        }
        // stage peaks for display
        for (int i = 0; i < n; ++i)
        {
            delayOutPeak = std::max (delayOutPeak, std::max (std::abs (sAL[i]), std::abs (sAR[i])));
            reverbOutPeak = std::max (reverbOutPeak, std::max (std::abs (sBL[i]), std::abs (sBR[i])));
            const float dm = 0.5f * (sAL[i] + sAR[i]), rm = 0.5f * (sBL[i] + sBR[i]);
            delayMin = std::min (delayMin, dm); delayMax = std::max (delayMax, dm);
            reverbMin = std::min (reverbMin, rm); reverbMax = std::max (reverbMax, rm);
        }
    }

    // ---- post wet chain
    if (p[WetLowCut] != lastLowCut) { lastLowCut = p[WetLowCut]; lowCutL.highpass (sr, lastLowCut); lowCutR.highpass (sr, lastLowCut); }
    if (p[WetHighCut] != lastHighCut) { lastHighCut = p[WetHighCut]; highCutL.lowpass (sr, lastHighCut); highCutR.lowpass (sr, lastHighCut); }
    const float width = p[Width] / 100.0f;
    const float duckAmt = p[DuckAmount] / 100.0f;
    const float duckA = onePoleCoef (p[DuckAttack] / 1000.0f, sr), duckR = onePoleCoef (p[DuckRelease] / 1000.0f, sr);
    const float limRel = t60Coef (0.25f, sr);
    // DRY / WET: 50% = both at unity; Wet Only overrides the blend (dry removed, wet at full level, mix value kept)
    wetGain.set (dbToGain (p[WetLevel]) * dbToGain (p[WetTrim]) * (p.b (WetOnly) ? 1.0f : mixWetGain (p[Mix])));
    dryGain.set (dbToGain (p[DryLevel]) * mixDryGain (p[Mix]));
    wetOnlyGain.set (p.b (WetOnly) ? 0.0f : 1.0f);
    const bool timingDip = pendingTopoKey >= 0 && (pendingTopoKey % 10) != (topoKey % 10);
    bool finite = allFinite (wl, n) && allFinite (wr, n);
    if (! finite)
    {
        zeroBuf (wl, n); zeroBuf (wr, n);
        telemetry.nonfiniteCount.fetch_add (1, std::memory_order_relaxed);
        hA.reset(); hB.reset();
        beginTailKill();
    }
    lastFinite = finite;
    bool prot = false;

    for (int i = 0; i < n; ++i)
    {
        const float xl = inL[i], xr = inR[i];
        // duck from the (undelayed) dry input envelope
        const float pk = std::max (std::abs (xl), std::abs (xr));
        duckEnv += (pk > duckEnv ? duckA : duckR) * (pk - duckEnv);
        float duckGain = 1.0f;
        if (duckAmt > 0.0f)
        {
            const float depthN = clampf ((gainToDb (duckEnv) + 50.0f) / 35.0f, 0.0f, 1.0f);
            duckGain = dbToGain (-24.0f * duckAmt * depthN);
        }
        lastDuckGain = duckGain;
        float l = lowCutL.process (wl[i]), r = lowCutR.process (wr[i]);
        l = highCutL.process (l); r = highCutR.process (r);
        const float m = 0.5f * (l + r), s = 0.5f * (l - r) * width;
        const float g = wetGain.next() * duckGain * dip * killGain;
        l = (m + s) * g; r = (m - s) * g;
        // wet protection limiter (dry untouched)
        const float apk = std::max (std::abs (l), std::abs (r));
        if (apk > limEnv) limEnv = apk; else limEnv *= limRel;
        if (limEnv > 0.89f) { const float lg = 0.89f / limEnv; l *= lg; r *= lg; if (lg < 0.97f) prot = true; }
        wl[i] = l; wr[i] = r;

        dryDelay.push (xl, xr);
        float dl = xl, dr = xr;
        if (studio) { dl = dryDelay.readL (studioLat); dr = dryDelay.readR (studioLat); }
        const float dg = dryGain.next() * wetOnlyGain.next() * (timingDip ? dip : 1.0f);
        outL[i] = dl * dg + l;
        outR[i] = dr * dg + r;

        // telemetry
        Telemetry::maxStore (telemetry.inPeakL, std::abs (xl));
        Telemetry::maxStore (telemetry.inPeakR, std::abs (xr));
        bucketDry = std::max (bucketDry, pk);
        bucketWet = std::max (bucketWet, apk);
        const int sw = telemetry.specWrite.load (std::memory_order_relaxed);
        telemetry.spec[(size_t) sw].store (0.5f * (l + r), std::memory_order_relaxed);
        telemetry.specWrite.store ((sw + 1) & (Telemetry::kSpecLen - 1), std::memory_order_relaxed);
        if (++bucketCount >= Telemetry::kBucket)
        {
            const int hw = telemetry.histWrite.load (std::memory_order_relaxed);
            telemetry.histDelay[(size_t) hw].store (delayOutPeak, std::memory_order_relaxed);
            telemetry.histReverb[(size_t) hw].store (reverbOutPeak, std::memory_order_relaxed);
            telemetry.histDelayMin[(size_t) hw].store (delayMin, std::memory_order_relaxed);
            telemetry.histDelayMax[(size_t) hw].store (delayMax, std::memory_order_relaxed);
            telemetry.histReverbMin[(size_t) hw].store (reverbMin, std::memory_order_relaxed);
            telemetry.histReverbMax[(size_t) hw].store (reverbMax, std::memory_order_relaxed);
            {
                const float lg = depthCur * lastDuckGain;
                for (int v = 0; v < Telemetry::kLanes; ++v)
                {
                    const auto& sl = voices.slot (v);
                    const bool sounding = sl.note >= 0 && sl.env > 1.0e-4f;
                    telemetry.histLaneMin[(size_t) v][(size_t) hw].store (sounding ? voiceTap.mn[v] * lg : 0.0f, std::memory_order_relaxed);
                    telemetry.histLaneMax[(size_t) v][(size_t) hw].store (sounding ? voiceTap.mx[v] * lg : 0.0f, std::memory_order_relaxed);
                    telemetry.histLaneNote[(size_t) v][(size_t) hw].store (sounding ? sl.note : -1, std::memory_order_relaxed);
                }
                voiceTap.clear();
            }
            delayMin = delayMax = reverbMin = reverbMax = 0.0f;
            telemetry.histWet[(size_t) hw].store (bucketWet, std::memory_order_relaxed);
            telemetry.histDry[(size_t) hw].store (bucketDry, std::memory_order_relaxed);
            telemetry.histWrite.store ((hw + 1) & (Telemetry::kHistLen - 1), std::memory_order_relaxed);
            bucketCount = 0; bucketWet = bucketDry = 0.0f; delayOutPeak = reverbOutPeak = 0.0f;
        }
    }
    float pkL = 0, pkR = 0, eWet = 0;
    for (int i = 0; i < n; ++i)
    {
        pkL = std::max (pkL, std::abs (outL[i])); pkR = std::max (pkR, std::abs (outR[i]));
        eWet += wl[i] * wl[i] + wr[i] * wr[i];
    }
    telemetry.wetEnergy.fetch_add (eWet, std::memory_order_relaxed);
    telemetry.wetEnergyCount.fetch_add (n, std::memory_order_relaxed);
    Telemetry::maxStore (telemetry.outPeakL, pkL);
    Telemetry::maxStore (telemetry.outPeakR, pkR);
    if (prot) { telemetry.protectionActive.store (true, std::memory_order_relaxed); telemetry.protectionCount.fetch_add (1, std::memory_order_relaxed); }
    voices.clearRetuned();
}
} // namespace pa
