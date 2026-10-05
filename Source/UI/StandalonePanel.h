// Standalone-only source & transport strip: Demo / File / Experiment, Live Input, MIDI capture, export, settings.
#pragma once

#include "Standalone/AuditionExporter.h"
#include "Standalone/SourcePlayer.h"
#include "Widgets.h"

namespace pa
{
class StandalonePanel : public juce::Component, private juce::Timer
{
public:
    explicit StandalonePanel (PluginProcessor& p);
    ~StandalonePanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void loadAudioFile (const juce::File& f);
    std::function<void()> onSaveExperiment;

private:
    struct SeekBar : public juce::Component
    {
        SourcePlayer* src = nullptr;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent& e) override { mouseDrag (e); }
        void mouseDrag (const juce::MouseEvent& e) override;
    };
    void timerCallback() override;
    void chooseSource (int id);
    void exportAudition();
    PluginProcessor& proc;
    SourcePlayer& src;
    juce::ComboBox sourceBox;
    juce::TextButton loadFile { "Load File..." }, settings { "Audio / MIDI" }, keepTail { "Keep Tail" }, liveInput { "Live Input" },
        exportBtn { "Export WAV" }, saveExp { "Save Experiment" };
    juce::TextButton play { "PLAY DEMO" };
    IconButton stop { IconButton::Icon::Stop, "Stop (tail rings unless Keep Tail is off)" },
        loop { IconButton::Icon::Loop, "Loop the source" }, recMidi { IconButton::Icon::Record, "Record MIDI for audition export (bounded)" };
    juce::Slider level;
    SeekBar seek;
    juce::Label status;
    AuditionExporter exporter;
    std::unique_ptr<juce::FileChooser> chooser;
    double tailSeconds = 10.0;
};
} // namespace pa
