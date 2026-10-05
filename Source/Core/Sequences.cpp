#include "Sequences.h"

#include <cmath>

namespace pa
{
namespace
{
void chord (std::vector<TimedEvent>& ev, double t0, double t1, std::initializer_list<int> notes, int vel = 100)
{
    for (int n : notes) ev.push_back ({ t0, 0x90, (uint8_t) n, (uint8_t) vel });
    for (int n : notes) ev.push_back ({ t1, 0x80, (uint8_t) n, 0 });
}
void sortEvents (std::vector<TimedEvent>& ev)
{
    std::stable_sort (ev.begin(), ev.end(), [] (const TimedEvent& a, const TimedEvent& b) {
        if (a.time != b.time) return a.time < b.time;
        return (a.status & 0xF0) == 0x80 && (b.status & 0xF0) != 0x80; // note-offs first at equal times
    });
}
} // namespace

const std::vector<Experiment>& builtInExperiments()
{
    static const std::vector<Experiment> list = [] {
        std::vector<Experiment> v;
        {
            Experiment e; e.name = "C minor > A-flat > F minor";
            e.description = "Sing-stop-revoice: the phrase ends at ~3.3 s, then chords change over the remaining tail.";
            e.fixture = Fixture::HarmonicTone; e.duration = 11.0;
            chord (e.events, 0.0, 3.6, { 48, 51, 55 });
            chord (e.events, 3.6, 6.2, { 44, 48, 51 });
            chord (e.events, 6.2, 9.0, { 41, 44, 48 });
            e.markers = { 0.0, 3.6, 6.2, 9.0 };
            sortEvents (e.events); v.push_back (e);
        }
        {
            Experiment e; e.name = "Freeze / Revoice";
            e.description = "Phrase, freeze at 3.4 s, then four chord changes over the frozen texture; release at 10 s.";
            e.fixture = Fixture::BreathNoise; e.duration = 12.0;
            chord (e.events, 0.0, 4.4, { 48, 51, 55 });
            e.events.push_back ({ 3.4, kSeqStatus, SeqFreezeOn, 0 });
            chord (e.events, 4.4, 5.9, { 44, 48, 51 });
            chord (e.events, 5.9, 7.4, { 41, 44, 48 });
            chord (e.events, 7.4, 8.9, { 43, 46, 50 });
            chord (e.events, 8.9, 10.0, { 48, 52, 55 });
            e.events.push_back ({ 10.0, kSeqStatus, SeqFreezeOff, 0 });
            e.markers = { 0.0, 3.4, 4.4, 5.9, 7.4, 8.9, 10.0 };
            sortEvents (e.events); v.push_back (e);
        }
        {
            Experiment e; e.name = "Sustain Pedal";
            e.description = "Notes played and released under the sustain pedal; pedal up at 5 s.";
            e.fixture = Fixture::BowedTexture; e.duration = 9.0;
            e.events.push_back ({ 0.0, 0xB0, 64, 127 });
            chord (e.events, 0.1, 0.6, { 50 });
            chord (e.events, 0.7, 1.2, { 57 });
            chord (e.events, 1.3, 1.8, { 62 });
            e.events.push_back ({ 5.0, 0xB0, 64, 0 });
            e.markers = { 0.0, 5.0 };
            sortEvents (e.events); v.push_back (e);
        }
        {
            Experiment e; e.name = "Four to One";
            e.description = "Four-note chord, then a single note, then four notes again (chord-size normalisation).";
            e.fixture = Fixture::BreathNoise; e.duration = 9.0;
            chord (e.events, 0.0, 2.5, { 48, 52, 55, 59 });
            chord (e.events, 2.5, 5.0, { 55 });
            chord (e.events, 5.0, 7.5, { 45, 48, 52, 55 });
            e.markers = { 0.0, 2.5, 5.0, 7.5 };
            sortEvents (e.events); v.push_back (e);
        }
        {
            Experiment e; e.name = "Held Chord (for Arp)";
            e.description = "Holds C minor 7 for the arpeggiator (select the Arp source).";
            e.fixture = Fixture::PluckedSequence; e.duration = 9.0;
            chord (e.events, 0.0, 8.0, { 48, 51, 55, 58 });
            e.markers = { 0.0, 8.0 };
            sortEvents (e.events); v.push_back (e);
        }
        {
            Experiment e; e.name = "No Keys";
            e.description = "Source only, no MIDI: shows the current no-note policy / stored chord.";
            e.fixture = Fixture::HarmonicTone; e.duration = 8.0;
            v.push_back (e);
        }
        return v;
    }();
    return list;
}

int eventsInWindow (const std::vector<TimedEvent>& ev, double t0, double t1, double sr, MidiEvent* out, int maxOut, Origin origin)
{
    int c = 0;
    const long long bs = std::llround (t0 * sr), be = std::llround (t1 * sr);
    for (const auto& e : ev)
    {
        const long long es0 = std::llround (e.time * sr);
        if (es0 < bs) continue;
        if (es0 >= be) break;
        if (c >= maxOut) break;
        MidiEvent m;
        // integer sample positions avoid block-size dependent rounding
        const long long es = std::llround (e.time * sr), bs = std::llround (t0 * sr);
        m.offset = (int) std::max (0LL, es - bs);
        m.status = e.status; m.d1 = e.d1; m.d2 = e.d2; m.origin = origin;
        out[c++] = m;
    }
    return c;
}
} // namespace pa
