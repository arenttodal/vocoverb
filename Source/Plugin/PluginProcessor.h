// Playable Ambience — JUCE AudioProcessor wrapping the shared pa::Engine (VST3, AU music effect, AU effect, standalone).
#pragma once

#include <JuceHeader.h>

#include "Core/Engine.h"
#include "Presets/FactoryPresets.h"

#include <array>
#include <atomic>
#include <memory>

namespace pa
{
class PresetManager;
class SourcePlayer;

class PluginProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    /** Host bypass: dry input delayed by the reported latency (keeps host delay compensation correct). */
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return JucePlugin_WantsMidiInput != 0; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ------------------------------------------------------------------ UI / app API (message thread)
    juce::AudioProcessorValueTreeState apvts;
    Telemetry& telemetry() noexcept { return engine.telemetry; }
    juce::RangedAudioParameter* param (int index) const noexcept { return params[(size_t) index]; }
    float value (int index) const noexcept { return raw[(size_t) index]->load(); }
    void setValue (int index, float realValue);        // with gesture, notifies host
    ParamSet currentParams() const;
    void applyParams (const ParamSet& ps, bool includeHostLocal); // preset application (gestures + engine note reset)

    void sendKeyboardMidi (uint8_t status, uint8_t d1, uint8_t d2) noexcept; // on-screen keys, wheels
    void panic() noexcept { engine.requestPanic(); }
    void tailKill() noexcept { engine.requestTailKill(); }

    std::vector<ChordSnapshot> snapshots;
    void storeSnapshot (int slot);
    void recallSnapshot (int slot);

    PresetManager& presets() noexcept { return *presetManager; }
    SourcePlayer* source() noexcept { return sourcePlayer.get(); }
    bool isStandalone() const noexcept { return wrapperType == wrapperType_Standalone; }
    bool isCompanion() const noexcept { return PA_COMPANION != 0; }
    bool hostIsPlaying() const noexcept { return lastHostPlaying.load(); }
    bool latencyChangePending() const noexcept { return pendingLatency.load(); }
    int reportedLatency() const noexcept { return getLatencySamples(); }
    int studioLatencyFor (int quality) const noexcept { return engine.studioLatency (quality); }
    double currentSampleRate() const noexcept { return preparedRate; }
    int currentBlockSize() const noexcept { return preparedBlock; }
    bool liveInputEnabled() const noexcept { return liveInput.load(); }
    void setLiveInputEnabled (bool b) noexcept { liveInput.store (b); }

    struct CpuStats { double avg = 0, p95 = 0, p99 = 0, max = 0; int count = 0; };
    CpuStats cpuStats() const; // fraction of the buffer deadline
    juce::var diagnostics() const;
    juce::String buildArchitecture() const;

    /** Measured rolling wet RMS (for A/B loudness match). */
    float wetRmsDb() const noexcept { return wetRmsDbValue.load(); }

    juce::String uiAdvancedTab = "Harmony";
    bool clearTailOnAB = false;

private:
    static BusesProperties makeBuses();
    void timerCallback() override;
    template <typename T> void processAny (juce::AudioBuffer<T>& buffer, juce::MidiBuffer& midi);
    template <typename T> void bypassAny (juce::AudioBuffer<T>& buffer, juce::MidiBuffer& midi);
    std::vector<float> bypassLine[2];
    int bypassWrite = 0;
    void processSlice (const float* inL, const float* inR, float* outL, float* outR, int n, const MidiEvent* ev, int nev, const TransportInfo& tp);
    std::unique_ptr<juce::XmlElement> stateToXml() const;
    void stateFromXml (const juce::XmlElement& xml);

    Engine engine;
    std::array<juce::RangedAudioParameter*, kNumParams> params {};
    std::array<std::atomic<float>*, kNumParams> raw {};
    std::unique_ptr<PresetManager> presetManager;
    std::unique_ptr<SourcePlayer> sourcePlayer;

    // keyboard / UI MIDI FIFO (single producer: message thread)
    juce::AbstractFifo kbFifo { 1024 };
    std::array<MidiEvent, 1024> kbEvents {};

    std::vector<MidiEvent> events;
    std::vector<float> inL, inR, outL, outR, srcL, srcR;
    double preparedRate = 48000.0;
    int preparedBlock = 512;
    int appliedQuality = 1, appliedTiming = 1;
    std::atomic<bool> pendingLatency { false }, lastHostPlaying { false }, liveInput { false };
    std::atomic<int> desiredLatency { 0 };

    // CPU telemetry: ring of block load ratios
    static constexpr int kCpuRing = 2048;
    std::array<std::atomic<float>, kCpuRing> cpuRing {};
    std::atomic<int> cpuWrite { 0 };
    double wetSumSq = 0.0; int wetCount = 0;
    std::atomic<float> wetRmsDbValue { -120.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
} // namespace pa
