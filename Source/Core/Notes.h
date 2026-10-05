// Playable Ambience — note handling: MIDI merge, latch/hold/sustain, chord & interval sources, arp, voice bank.
#pragma once

#include "DspUtil.h"
#include "Params.h"

#include <array>
#include <cstdint>

namespace pa
{
enum class Origin : uint8_t { Host = 0, Keyboard = 1, Sequence = 2 };

struct MidiEvent
{
    int offset = 0;       // sample offset inside the processed block
    uint8_t status = 0;   // full status byte incl. channel
    uint8_t d1 = 0, d2 = 0;
    Origin origin = Origin::Host;
};

constexpr int kMaxVoices = 6;
constexpr int kMaxNotes = 32;

struct NoteEntry { int8_t note = -1; uint8_t vel = 0; uint32_t order = 0; };

/** Small fixed-capacity ordered note set. */
struct NoteSet
{
    std::array<NoteEntry, kMaxNotes> e {};
    int n = 0;
    void clear() noexcept { n = 0; }
    bool contains (int note) const noexcept { for (int i = 0; i < n; ++i) if (e[(size_t) i].note == note) return true; return false; }
    void add (int note, int vel, uint32_t order) noexcept
    {
        for (int i = 0; i < n; ++i) if (e[(size_t) i].note == note) { e[(size_t) i].vel = (uint8_t) vel; return; }
        if (n < kMaxNotes) e[(size_t) n++] = { (int8_t) note, (uint8_t) vel, order };
    }
    void remove (int note) noexcept
    {
        for (int i = 0; i < n; ++i)
            if (e[(size_t) i].note == note) { for (int j = i; j < n - 1; ++j) e[(size_t) j] = e[(size_t) j + 1]; --n; return; }
    }
    int lowest() const noexcept { int lo = 999; for (int i = 0; i < n; ++i) lo = std::min (lo, (int) e[(size_t) i].note); return n ? lo : -1; }
    bool sameNotes (const NoteSet& o) const noexcept
    {
        if (o.n != n) return false;
        for (int i = 0; i < n; ++i) if (! o.contains (e[(size_t) i].note)) return false;
        return true;
    }
};

/** Merges live MIDI from all origins: duplicate counts per origin, sustain, latch, hold-last. */
class NoteManager
{
public:
    void reset() noexcept;
    /** Handles a note-on/off. Returns true if the effective set may have changed. */
    void noteOn (Origin o, int ch, int note, int vel, double timeSec) noexcept;
    void noteOff (Origin o, int ch, int note, double timeSec) noexcept;
    void sustain (int ch, bool down, double timeSec) noexcept;
    void allNotesOff() noexcept;      // releases everything incl. latch and hold
    void allNotesOff (Origin o) noexcept; // releases one origin's notes; clears latch/hold when nothing remains held
    void setLatch (bool on) noexcept;
    void setPolicy (int noNotePolicy) noexcept { policy = noNotePolicy; }
    void setChordWindow (double sec) noexcept { chordWindow = sec; }

    /** Effective MIDI note set after sustain/latch/hold policies. */
    const NoteSet& effective() const noexcept { return eff; }
    /** True if the last change replaced a held/latched chord (use the Transition fade). */
    bool consumeReplacement() noexcept { const bool r = replacedFlag; replacedFlag = false; return r; }
    bool physicalEmpty() const noexcept { return physCount() == 0; }
    bool isHolding() const noexcept { return holdingLast; }
    int physCount() const noexcept;

private:
    void recompute (double t) noexcept;
    struct Held { int8_t note = -1; uint8_t origin = 0, ch = 0, count = 0; bool sustained = false; uint8_t vel = 0; uint32_t order = 0; };
    std::array<Held, 64> held {};
    int numHeld = 0;
    std::array<bool, 16> sustainDown {};
    NoteSet latched, eff, lastChord;
    struct Rel { int8_t note; uint8_t vel; double t; uint32_t order; };
    std::array<Rel, 16> recentRel {};
    int numRecentRel = 0;
    bool latch = false, holdingLast = false, replacedFlag = false;
    int policy = 0;
    double chordWindow = 0.03, lastReplaceTime = -1.0e9;
    uint32_t orderCounter = 1;
    int prevPhys = 0;
};

/** Build stored chord / interval note sets from parameters. */
void buildChord (const ParamSet& p, NoteSet& out) noexcept;
void buildIntervals (const ParamSet& p, int root, int velocity, NoteSet& out) noexcept;

/** Host-synced or free arpeggiator producing single gated notes. */
class Arpeggiator
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); }
    void reset() noexcept { stepIndex = -1; gateOn = false; curNote = -1; samplePos = 0; lastPpqStep = -1; rng = Rng (0xA55A1234u); dirUp = true; seqPos = -1; }
    struct Out { bool changed = false; int note = -1; int vel = 100; bool gate = false; };
    /** Advance one sample. ppq < 0 means free-running. Returns note changes. */
    Out tick (const ParamSet& p, const NoteSet& pool, double ppq, double bpm) noexcept;
    int currentNote() const noexcept { return gateOn ? curNote : -1; }

private:
    int pickNext (const ParamSet& p, const NoteSet& pool) noexcept;
    double sr = 48000.0;
    long long stepIndex = -1, lastPpqStep = -1;
    double samplePos = 0;      // free-running sample counter
    double gateOffAt = -1;     // position (in steps) where gate ends
    bool gateOn = false, dirUp = true;
    int curNote = -1, curVel = 100, seqPos = -1;
    Rng rng { 0xA55A1234u };
};

/** Fixed bank of harmony voices with glide, envelopes, stealing and energy normalisation. */
class VoiceBank
{
public:
    struct Slot
    {
        int note = -1;          // current note (-1 free)
        int pendingNote = -1;   // note to assign after steal fade
        float pendingVel = 0;
        bool gate = false;
        float vel = 0.0f;       // velocity gain
        float env = 0.0f;       // envelope 0..1
        float relCoef = 0.999f; // per-sample release multiplier
        float logHz = 0.0f, targetLogHz = 0.0f, glideCoef = 1.0f;
        float steal = 1.0f;     // steal fade gain
        bool stealing = false;
        bool retuned = false;   // set when the slot changed note abruptly (methods may reset state)
        uint32_t age = 0;
    };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    /** Reconcile with the desired set. replacement => departing voices use the transition time. */
    void setDesired (const NoteSet& desired, bool replacement, const ParamSet& p) noexcept;
    void killAll (float fadeMs) noexcept;
    /** Renders per-sample gain (env*vel*norm*steal), frequency and steal fade for n <= 64 samples. */
    void render (int n, const ParamSet& p, float bendSemis) noexcept;

    static constexpr int kChunk = 64;
    float gain[kMaxVoices][kChunk] {};   // excitation/carrier gain
    float hz[kMaxVoices][kChunk] {};     // instantaneous frequency
    float outFade[kMaxVoices][kChunk] {};// steal fade only (for resonant state output)
    const Slot& slot (int i) const noexcept { return slots[(size_t) i]; }
    Slot& slotMut (int i) noexcept { return slots[(size_t) i]; }
    float activity() const noexcept;     // sum of envelopes (for ambient policy / display)
    void clearRetuned() noexcept { for (auto& s : slots) s.retuned = false; }
    int lowestGatedNote() const noexcept;

private:
    int allocateSlot (int polyphony) noexcept;
    std::array<Slot, kMaxVoices> slots {};
    double sr = 48000.0;
    uint32_t ageCounter = 1;
    float normSmoothed = 1.0f, normCoef = 0.001f, a4 = 440.0f;
    float bendSmoothed = 0.0f;
    float attackMs = 15.0f;
};

} // namespace pa
