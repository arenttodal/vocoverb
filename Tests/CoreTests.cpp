// Parameter table, note manager, chord/interval builders, arp and voice bank tests.
#include "TestUtil.h"

using namespace pa;
using namespace pat;

TEST ("params: ids unique, defaults within range, normalise roundtrip")
{
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& p = paramInfo (i);
        CHECK_MSG (paramIndexForId (p.id) == i, p.id);
        CHECK_MSG (p.def >= p.minV && p.def <= p.maxV, p.id);
        for (float t : { 0.0f, 0.13f, 0.5f, 0.77f, 1.0f })
        {
            const float v = paramFromNormalised (i, t);
            const float back = paramFromNormalised (i, paramToNormalised (i, v));
            CHECK_MSG (std::abs (back - v) <= 1.0e-3f * std::max (1.0f, std::abs (v)), p.id);
        }
        // text roundtrip of the default
        float parsed = 0.0f;
        const auto txt = paramValueToText (i, p.def);
        if (paramTextToValue (i, txt, parsed))
            CHECK_MSG (std::abs (parsed - p.def) <= 0.051f * std::max (1.0f, std::abs (p.def)), std::string (p.id) + " text " + txt);
    }
    float v = 0;
    CHECK (paramTextToValue (BbdTone, "3.2 kHz", v) && std::abs (v - 3200.0f) < 1.0f);
    CHECK (paramTextToValue (ShRef, "C3", v) && (int) v == 48);
    CHECK (paramValueToText (DryLevel, -60.0f) == "-inf dB");
}

TEST ("notes: velocity-zero note-on is note-off; duplicate origins counted separately")
{
    NoteManager nm; nm.reset(); nm.setPolicy (1);
    nm.noteOn (Origin::Host, 0, 60, 100, 0.0);
    nm.noteOn (Origin::Keyboard, 0, 60, 90, 0.01);
    CHECK (nm.effective().n == 1);
    nm.noteOn (Origin::Host, 0, 60, 0, 0.02); // vel 0 => off for host instance only
    CHECK (nm.effective().contains (60));
    nm.noteOff (Origin::Keyboard, 0, 60, 0.03);
    CHECK (nm.effective().n == 0);
    // same origin, overlapping instances of the same pitch
    nm.noteOn (Origin::Host, 0, 62, 100, 0.1);
    nm.noteOn (Origin::Host, 0, 62, 100, 0.11);
    nm.noteOff (Origin::Host, 0, 62, 0.12);
    CHECK (nm.effective().contains (62));
    nm.noteOff (Origin::Host, 0, 62, 0.13);
    CHECK (! nm.effective().contains (62));
}

TEST ("notes: sustain pedal holds notes without losing note-offs")
{
    NoteManager nm; nm.reset(); nm.setPolicy (1);
    nm.sustain (0, true, 0.0);
    nm.noteOn (Origin::Host, 0, 60, 100, 0.0);
    nm.noteOff (Origin::Host, 0, 60, 0.1);
    CHECK (nm.effective().contains (60));
    nm.noteOn (Origin::Host, 0, 64, 100, 0.2);
    nm.sustain (0, false, 0.3);
    CHECK (! nm.effective().contains (60));
    CHECK (nm.effective().contains (64)); // still physically held
    nm.noteOff (Origin::Host, 0, 64, 0.4);
    CHECK (nm.effective().n == 0);
}

TEST ("notes: hold last keeps the released chord; next note replaces it")
{
    NoteManager nm; nm.reset(); nm.setPolicy (0);
    for (int n : { 48, 51, 55 }) nm.noteOn (Origin::Host, 0, n, 100, 0.0);
    nm.noteOff (Origin::Host, 0, 48, 1.00);
    nm.noteOff (Origin::Host, 0, 51, 1.02);
    nm.noteOff (Origin::Host, 0, 55, 1.04);
    CHECK (nm.effective().n == 3);
    CHECK (nm.isHolding());
    nm.noteOn (Origin::Host, 0, 44, 100, 2.0);
    CHECK (nm.consumeReplacement());
    CHECK (nm.effective().n == 1 && nm.effective().contains (44));
}

TEST ("notes: latch replacement and sustain-off cannot stick a latch")
{
    NoteManager nm; nm.reset(); nm.setPolicy (1); nm.setChordWindow (0.03);
    nm.setLatch (true);
    nm.noteOn (Origin::Host, 0, 48, 100, 0.0); nm.noteOn (Origin::Host, 0, 52, 100, 0.005);
    nm.noteOff (Origin::Host, 0, 48, 0.5); nm.noteOff (Origin::Host, 0, 52, 0.5);
    CHECK (nm.effective().n == 2);
    nm.sustain (0, true, 0.6); nm.sustain (0, false, 0.7);
    CHECK (nm.effective().n == 2);
    // new chord after all keys up replaces; second note inside the chord window joins even though first was released
    nm.noteOn (Origin::Host, 0, 55, 100, 1.0); nm.noteOff (Origin::Host, 0, 55, 1.01);
    nm.noteOn (Origin::Host, 0, 59, 100, 1.02);
    CHECK (nm.effective().n == 2 && nm.effective().contains (55) && nm.effective().contains (59));
    nm.allNotesOff();
    CHECK (nm.effective().n == 0);
    nm.setLatch (false);
    CHECK (nm.effective().n == 0);
}

TEST ("chords: qualities, inversion, voicing; intervals chromatic vs scale")
{
    ParamSet p;
    p[ChRoot] = 0; p[ChOctave] = 3; p[ChQuality] = 1; p[ChInversion] = 0; p[ChSpread] = 0;
    NoteSet s; buildChord (p, s);
    CHECK (s.n == 3 && s.contains (48) && s.contains (51) && s.contains (55));
    p[ChInversion] = 1; buildChord (p, s);
    CHECK (s.contains (60) && ! s.contains (48));
    p[ChInversion] = 0; p[ChQuality] = 5; buildChord (p, s);
    CHECK (s.n == 4 && s.contains (59));
    p[ChQuality] = 8; buildChord (p, s);
    CHECK (s.n == 3 && s.contains (51)); // custom defaults 48 51 55
    // intervals: chromatic +4 vs scale third (C major: 2 steps = E (+4), D dorian etc.)
    p[IntMode] = 0; p[IntCount] = 2; p[Int1] = 0; p[Int2] = 4;
    buildIntervals (p, 50, 100, s);
    CHECK (s.contains (54));
    p[IntMode] = 1; p[IntKey] = 0; p[IntScale] = 0; p[Int2] = 2; // scale third above D in C major = F (+3)
    buildIntervals (p, 50, 100, s);
    CHECK (s.contains (53) && ! s.contains (54));
}

TEST ("arp: up / down / up-down order and gate")
{
    ParamSet p; p[ArpSync] = 0; p[ArpFreeRate] = 10; p[ArpGate] = 50; p[ArpOctaves] = 1;
    NoteSet pool; pool.add (60, 100, 1); pool.add (64, 100, 2); pool.add (67, 100, 3);
    auto seqFor = [&] (int mode) {
        p[ArpMode] = (float) mode;
        Arpeggiator a; a.prepare (1000.0);
        std::vector<int> got;
        for (int i = 0; i < 700; ++i) { auto o = a.tick (p, pool, -1.0, 120); if (o.changed && o.gate) got.push_back (o.note); }
        return got;
    };
    auto up = seqFor (0);
    CHECK (up.size() >= 6 && up[0] == 60 && up[1] == 64 && up[2] == 67 && up[3] == 60);
    auto dn = seqFor (1);
    CHECK (dn[0] == 67 && dn[1] == 64 && dn[2] == 60);
    auto ud = seqFor (2);
    CHECK (ud[0] == 60 && ud[1] == 64 && ud[2] == 67 && ud[3] == 64 && ud[4] == 60);
    // gate: note off occurs mid-step
    p[ArpMode] = 0;
    Arpeggiator a; a.prepare (1000.0);
    int ons = 0, offs = 0;
    for (int i = 0; i < 1000; ++i) { auto o = a.tick (p, pool, -1.0, 120); if (o.changed) { if (o.gate) ++ons; else ++offs; } }
    CHECK (ons >= 9 && offs >= 9);
}

TEST ("voices: seventh note steals (quiet releasing first, else oldest); no silent discard")
{
    VoiceBank vb; vb.prepare (48000.0);
    ParamSet p; p[Polyphony] = 6;
    NoteSet d;
    for (int k = 0; k < 6; ++k) d.add (48 + k * 2, 100, (uint32_t) k + 1);
    vb.setDesired (d, false, p);
    for (int b = 0; b < 40; ++b) vb.render (64, p, 0.0f);
    d.add (70, 100, 99);
    vb.setDesired (d, false, p);
    for (int b = 0; b < 10; ++b) vb.render (64, p, 0.0f);
    bool has70 = false, has48 = false;
    for (int v = 0; v < kMaxVoices; ++v) { if (vb.slot (v).note == 70) has70 = true; if (vb.slot (v).note == 48) has48 = true; }
    CHECK_MSG (has70, "seventh note must be voiced");
    CHECK_MSG (! has48, "oldest voice should have been stolen");
    // energy normalisation bounded across chord sizes
    float g6 = 0; for (int v = 0; v < kMaxVoices; ++v) g6 += vb.gain[v][63] * vb.gain[v][63];
    NoteSet one; one.add (60, 100, 1);
    vb.setDesired (one, true, p);
    for (int b = 0; b < 200; ++b) vb.render (64, p, 0.0f);
    float g1 = 0; for (int v = 0; v < kMaxVoices; ++v) g1 += vb.gain[v][63] * vb.gain[v][63];
    metric ("voices.energy6", g6); metric ("voices.energy1", g1);
    CHECK (g6 > 0.5f && g6 < 1.6f && g1 > 0.5f && g1 < 1.2f);
}

TEST ("factory presets: 16+ presets, all ids valid")
{
    const auto& fp = factoryPresets();
    CHECK (fp.size() >= 16);
    for (const auto& pr : fp)
        for (const auto& kv : pr.values) CHECK_MSG (paramIndexForId (kv.first) >= 0, pr.name + ": " + kv.first);
}

TEST ("params: Classic-only migration and Dry/Wet law")
{
    // legacy Off -> Harmony Enable off (method stored as Classic); FFT/Resonator/Shift -> Classic
    for (int m = 0; m < 5; ++m)
    {
        ParamSet ps; ps[HarmMethod] = (float) m; ps[HarmEnable] = 1;
        migrateLegacyHarmony (ps);
        CHECK (ps.i (HarmMethod) == 1);
        CHECK (ps.b (HarmEnable) == (m != 0));
        CHECK (effectiveHarmonyMethod (ps) == (m != 0 ? 1 : 0));
    }
    ParamSet off; off[HarmEnable] = 0;
    CHECK (effectiveHarmonyMethod (off) == 0);
    // mix law: endpoints exact, 50% = original unity blend, continuous and monotonic
    CHECK (mixDryGain (0) == 1.0f && mixWetGain (0) == 0.0f);
    CHECK (mixDryGain (100) == 0.0f && mixWetGain (100) == 1.0f);
    CHECK (mixDryGain (50) == 1.0f && mixWetGain (50) == 1.0f);
    CHECK (std::abs (mixWetGain (35) - 0.7f) < 1e-6f && mixDryGain (35) == 1.0f);
    float pd = 2, pw = -1;
    for (int k = 0; k <= 100; ++k) { CHECK (mixDryGain ((float) k) <= pd && mixWetGain ((float) k) >= pw); pd = mixDryGain ((float) k); pw = mixWetGain ((float) k); }
    CHECK (paramInfo (Mix).def == 50.0f); // sessions saved before the mix parameter keep their blend
}
