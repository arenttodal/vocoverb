// Standalone audition source: synthetic demos, decoded files and repeatable experiments, with transport,
// loop fades, sequence MIDI and bounded MIDI capture. Audio-thread side is lock-free; buffers are prepared on
// background/message threads and handed over through atomic pointers.
#pragma once

#include <JuceHeader.h>

#include "Core/Sequences.h"

#include <atomic>
#include <memory>

namespace pa
{
class SourcePlayer
{
public:
    enum class Kind { Demo = 0, File = 1, Experiment = 2 };
    struct Data
    {
        juce::AudioBuffer<float> audio;
        double sampleRate = 48000.0;
        std::vector<TimedEvent> events;
        std::vector<double> markers;
        juce::String name, info;
        Kind kind = Kind::Demo;
        int index = 0;
    };

    SourcePlayer();
    ~SourcePlayer();

    void prepare (double sampleRate, int maxBlock);

    // ---- message thread
    void selectDemo (int fixture);
    void selectExperiment (int index);
    /** Decodes on a background thread; result arrives asynchronously. Returns false if obviously unsupported. */
    bool loadFile (const juce::File& f, juce::String& error);
    void play();
    void pause();
    void stop();
    void seek (double seconds);
    void setLoop (bool b) noexcept { loop.store (b); }
    bool isLooping() const noexcept { return loop.load(); }
    void setLevelDb (float db) noexcept { level.store (juce::Decibels::decibelsToGain (db, -60.0f)); }
    void setKeepTailOnStop (bool b) noexcept { keepTail.store (b); }
    bool keepTailOnStop() const noexcept { return keepTail.load(); }
    bool isPlaying() const noexcept { return playing.load(); }
    double positionSeconds() const noexcept;
    double durationSeconds() const noexcept;
    std::shared_ptr<const Data> currentData() const { return lastSubmitted; }
    juce::String statusText() const { return status; }
    juce::String loadingText() const { return loadingMessage; }
    void messageThreadHousekeeping();

    // MIDI capture for audition export (bounded)
    void setRecording (bool b);
    bool isRecording() const noexcept { return recording.load(); }
    std::vector<TimedEvent> recordedEvents() const;
    int recordedCount() const noexcept { return recCount.load(); }

    // ---- audio thread
    void render (float* L, float* R, int n, MidiEvent* outEvents, int& numOut, int maxOut, bool& requestTailKill) noexcept;
    void captureMidi (const MidiEvent* ev, int n) noexcept;

    static constexpr double kMaxFileSeconds = 120.0;

private:
    void submit (std::unique_ptr<Data> d);
    std::unique_ptr<Data> makeDemo (int fixture) const;
    double sr = 48000.0;
    std::atomic<Data*> pending { nullptr }, retired { nullptr };
    Data* active = nullptr;                 // audio thread owned
    std::shared_ptr<const Data> lastSubmitted;
    std::atomic<bool> playing { false }, loop { true }, keepTail { true }, recording { false };
    std::atomic<int> cmd { 0 };            // 1 play, 2 pause, 3 stop
    std::atomic<long long> seekTo { -1 }, posAtomic { 0 };
    std::atomic<float> level { 1.0f };
    long long pos = 0;
    float fade = 0.0f;
    bool wasPlaying = false;
    juce::String status, loadingMessage;
    std::unique_ptr<juce::ThreadPool> pool;
    std::atomic<int> loadGeneration { 0 };

    static constexpr int kRecMax = 8192;
    std::array<TimedEvent, kRecMax> rec {};
    std::atomic<int> recCount { 0 };
    std::atomic<long long> recStartPos { 0 };
};
} // namespace pa
