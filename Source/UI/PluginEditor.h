// Playable Ambience editor: ivory/orange interface following the supplied mockup, every control live.
#pragma once

#include "AdvancedPanel.h"
#include "Graphs.h"
#include "Keyboard.h"
#include "LookAndFeel.h"
#include "Widgets.h"

namespace pa
{
class StandalonePanel;
class MainView;

class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer, public juce::FileDragAndDropTarget
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    bool keyPressed (const juce::KeyPress&) override;

    void openAdvanced (const juce::String& tab);
    void closeAdvanced();
    void saveExperiment();
    /** Used by the headless self-test to produce screenshots with populated graphs. */
    void refreshForSnapshot();
    void tickForSnapshot() { timerCallback(); }
    void showAdvancedForSnapshot (const juce::String& tab);

private:
    void timerCallback() override;
    PluginProcessor& proc;
    PALookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 650 };
    std::unique_ptr<StandalonePanel> standalone;
    juce::Viewport viewport;
    std::unique_ptr<MainView> view;
    std::unique_ptr<AdvancedPanel> advanced;
    juce::Component::SafePointer<juce::Component> focusBeforeAdvanced;
    std::unique_ptr<juce::FileChooser> chooser;
    int frame = 0;
};
} // namespace pa
