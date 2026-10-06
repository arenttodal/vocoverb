// Playable Ambience editor. The main view is laid out once on the canonical 1536 x 1024 canvas of
// references/playable-ambience-approved-gui.png and the whole canvas is scaled uniformly with the window
// (fixed aspect ratio). The standalone source strip sits above the canvas inside the same scaled container.
#pragma once

#include "AdvancedPanel.h"
#include "Keyboard.h"
#include "LookAndFeel.h"
#include "Visuals.h"
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
    void tickForSnapshot();
    void showAdvancedForSnapshot (const juce::String& tab);
    /** Visual-test only: deterministic art-directed graph data derived from the approved reference. */
    void setReferenceFixture (bool on);
    /** Renders only the canonical main view at 1536 x 1024 (times `scale`), without host chrome or the standalone strip. */
    juce::Image snapshotCanvas (float scale);
    juce::Component* mainView() const;
    std::unique_ptr<juce::Component> createSourceSettingsForSnapshot();

    static constexpr int kStripH = 82;
    static constexpr float kDefaultScale = 0.85f, kMinScale = 0.62f, kMaxScale = 1.5f;

private:
    void timerCallback() override;
    float currentScale() const;
    PluginProcessor& proc;
    PALookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };
    juce::Component canvas;                 // canonical coordinates; scaled by an affine transform
    std::unique_ptr<StandalonePanel> standalone;
    std::unique_ptr<MainView> view;
    std::unique_ptr<AdvancedPanel> advanced;
    juce::Component::SafePointer<juce::Component> focusBeforeAdvanced;
    std::unique_ptr<juce::FileChooser> chooser;
    int frame = 0;
};
} // namespace pa
