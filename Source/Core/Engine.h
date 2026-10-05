// Playable Ambience — the complete DSP graph shared by every wrapper (VST3, AU, standalone, offline tools).
#pragma once

#include "../DSP/Delay/Delays.h"
#include "../DSP/Harmony/HarmonyUnit.h"
#include "../DSP/Reverb/Reverbs.h"
#include "Notes.h"
#include "Params.h"
#include "Telemetry.h"

#include <atomic>

namespace pa
{
struct TransportInfo
{
    bool hasHost = false;   // host supplied tempo/position
    bool playing = false;
    double bpm = 120.0;
    double ppqAtBlockStart = -1.0;
};

/** Internal pseudo-MIDI used by experiment sequences (never forwarded from hosts). */
constexpr uint8_t kSeqStatus = 0xF4;
enum SeqCommand : uint8_t { SeqFreezeOn = 1, SeqFreezeOff = 2, SeqPanic = 3, SeqTailKill = 4 };

class Engine
{
public:
    Engine();
    /** Allocates everything. Not real-time safe. */
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    double sampleRate() const noexcept { return sr; }

    /** Real-time safe. Events must be sorted by offset. Buffers may alias (in == out allowed). */
    void process (const ParamSet& params, const TransportInfo& tp, const float* inL, const float* inR,
                  float* outL, float* outR, int numSamples, const MidiEvent* events, int numEvents) noexcept;

    /** Studio latency (samples) for a quality at the prepared sample rate. */
    int studioLatency (int quality) const noexcept;
    /** Commands (thread safe, applied at next block). */
    void requestPanic() noexcept { cmdPanic.store (true); }
    void requestTailKill() noexcept { cmdTailKill.store (true); }
    void requestStateReset() noexcept { cmdStateReset.store (true); } // after preset load: clear notes + stale capture, keep tails

    Telemetry telemetry;
    /** Diagnostics */
    bool lastBlockFinite() const noexcept { return lastFinite; }
    const VoiceBank& voiceBank() const noexcept { return voices; }
    int effectiveQuality() const noexcept { return appliedQuality; }
    static constexpr int kChunk = VoiceBank::kChunk;

private:
    struct Stage
    {
        int active = 0;
        float gain[2] { 0, 0 };
        int state[2] { 0, 0 };  // 0 idle, 1 active, 2 retiring, 3 clearing
        float inputGain = 1.0f;
        float level[2] { 1, 1 };
        float enable() const noexcept { return std::min (1.0f, gain[0] + gain[1]); }
    };
    void handleEvent (const MidiEvent& e) noexcept;
    void updateNotes (bool force) noexcept;
    void processChunk (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;
    /** Runs a stage (both modes with crossfades). raw = unity-level wet, lev = per-mode level applied. Returns enable factor. */
    float runDelay (const SpaceContext& sc, bool freeze, const float* inL, const float* inR, float* rawL, float* rawR, float* levL, float* levR, int n) noexcept;
    float runReverb (const SpaceContext& sc, const float* inL, const float* inR, float* rawL, float* rawR, float* levL, float* levR, int n) noexcept;
    void beginTailKill() noexcept;
    bool clearProgress (int budget) noexcept;

    double sr = 48000.0;
    int maxBlock = 512;
    ParamSet p;
    TransportInfo transport;
    double timeSec = 0.0, ppq = -1.0;

    NoteManager notes;
    VoiceBank voices;
    Arpeggiator arp;
    CarrierSynth carrier;
    NoteSet lastDesired, chordSet;
    bool midiEverReceived = false;
    bool notesDirty = true;
    float bendSemis = 0.0f, modWheel = 0.0f;
    float pitchDriver = 0.0f;
    int lastArpNote = -1;
    bool seqFreeze = false;

    BbdDelay bbd;
    IntervalDelay ivd;
    CaptureLooper delayLooper;
    PlateReverb plate;
    WashReverb wash;
    Stage dStage, rStage;
    HarmonyUnit hA, hB;

    // dry / post
    StereoDelay dryDelay;
    Biquad lowCutL, lowCutR, highCutL, highCutR;
    float lastLowCut = -1, lastHighCut = -1;
    float duckEnv = 0.0f;
    float limEnv = 0.0f;
    float depthCur = 0.0f, ambient = 1.0f;
    Smoothed dryGain, wetGain, wetOnlyGain;
    float dip = 1.0f;
    int dipState = 0; // 0 none, 1 fading out, 2 fading in
    int topoKey = -1, pendingTopoKey = -1, activeRouting = 0, activePlacement = 0, activeApply = 0;
    int appliedQuality = 1, pendingQuality = -1;
    bool studio = true, pendingStudio = true;
    int studioLat = 0;
    float dryDip = 1.0f;

    // tail kill / clearing
    int killState = 0; // 0 none, 1 fade out, 2 clearing, 3 fade in
    float killGain = 1.0f;
    std::atomic<bool> cmdPanic { false }, cmdTailKill { false }, cmdStateReset { false };
    bool lastFinite = true;
    bool needsInit = true;
    void initFromParams() noexcept;

    // telemetry accumulation
    int bucketCount = 0;
    float bucketDelay = 0, bucketReverb = 0, bucketWet = 0, bucketDry = 0;

    // scratch
    float carrierBuf[kChunk], carrierGain[kChunk];
    float sAL[kChunk], sAR[kChunk], sBL[kChunk], sBR[kChunk], sCL[kChunk], sCR[kChunk], sDL[kChunk], sDR[kChunk];
    float tmpL[kChunk], tmpR[kChunk], zero[kChunk], inScL[kChunk], inScR[kChunk];
    float sEL[kChunk], sER[kChunk], sFL[kChunk], sFR[kChunk];
    float wetL[kChunk], wetR[kChunk];
    float delayOutPeak = 0, reverbOutPeak = 0;
    // arp handling
    bool arpSkipFirstTick = false;
    Arpeggiator::Out pendingArp;
    bool hasPendingArp = false;
};
} // namespace pa
