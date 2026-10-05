// Repeatable experiment sequences (timed MIDI + freeze commands) and a bounded MIDI capture sequence.
#pragma once

#include "Engine.h"
#include "Fixtures.h"

#include <string>
#include <vector>

namespace pa
{
struct TimedEvent
{
    double time = 0.0;  // seconds from sequence start
    uint8_t status = 0, d1 = 0, d2 = 0;
};

struct Experiment
{
    std::string name, description;
    Fixture fixture = Fixture::HarmonicTone;
    double duration = 10.0;
    std::vector<TimedEvent> events;
    std::vector<double> markers; // seconds, shown on the standalone transport
};

/** Built-in experiments: chord progression, freeze/revoice, sustain pedal, four-to-one, arp hold, no keys. */
const std::vector<Experiment>& builtInExperiments();

/** Collects the events falling into [t0, t1) (seconds) with sample offsets for a block. */
int eventsInWindow (const std::vector<TimedEvent>& ev, double t0, double t1, double sampleRate, MidiEvent* out, int maxOut, Origin origin);

} // namespace pa
