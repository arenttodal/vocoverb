// Offline audition render on a background thread: a fresh Engine renders the chosen source segment with a
// parameter snapshot and an explicit MIDI/chord sequence, plus a bounded tail, to a 24-bit WAV.
#pragma once

#include <JuceHeader.h>

#include "Core/Sequences.h"

#include <atomic>

namespace pa
{
class AuditionExporter : private juce::Thread
{
public:
    struct Job
    {
        ParamSet params;
        juce::AudioBuffer<float> source;
        double sampleRate = 48000.0;
        std::vector<TimedEvent> events;
        double tailSeconds = 10.0;
        juce::File output;
        juce::String description;
    };
    static constexpr double kMaxTailSeconds = 60.0;
    static constexpr double kMaxTotalSeconds = 240.0;

    AuditionExporter() : juce::Thread ("PA audition export") {}
    ~AuditionExporter() override { cancel(); stopThread (5000); }

    bool start (Job&& j, juce::String& error);
    void cancel() { cancelled.store (true); }
    bool isBusy() const { return isThreadRunning(); }
    float progress() const { return prog.load(); }
    juce::String result() const { const juce::ScopedLock sl (lock); return resultText; }
    std::function<void (bool ok, juce::String message)> onFinished;

    /** Synchronous render used by tests/tools. */
    static bool renderToBuffer (const Job& job, juce::AudioBuffer<float>& out, std::atomic<bool>* cancel, std::atomic<float>* progress);

private:
    void run() override;
    Job job;
    std::atomic<bool> cancelled { false };
    std::atomic<float> prog { 0.0f };
    juce::CriticalSection lock;
    juce::String resultText;
};
} // namespace pa
