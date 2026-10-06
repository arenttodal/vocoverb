// Playable Ambience editor. The main view is laid out once on the canonical 1536 x 1024 canvas of
// references/playable-ambience-approved-gui.png and the whole canvas is scaled uniformly with the window
// (fixed aspect ratio). The standalone source strip sits above the canvas inside the same scaled container.
#pragma once

#include "Keyboard.h"
#include "SettingsViews.h"
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

    /** Contextual settings in place of an effect graph: 0 delay, 1 reverb, 2 harmony, -1 back to all graphs. */
    void showEffectSettings (int effect, int page = -1);
    /** Compact plugin-wide Settings near the header gear: 0 MIDI / Setup, 1 Audio, 2 Support; -1 closes. */
    void showSettings (int page);
    void toggleSettings();
    bool settingsOpen() const noexcept { return settings != nullptr; }
    int openEffectSettings() const;                 // -1 when every effect shows its graph
    juce::String effectSettingsTitle (int effect) const;
    /** Routes a named destination ("Delay", "Reverb", "Harmony", "MIDI", "Audio", "Support", or the retired
        Advanced tab names "Mix / Timing" and "Diagnostics") to its new view; empty closes everything. */
    void showDestination (const juce::String& name);
    void saveExperiment();
    /** Used by the headless self-test to produce screenshots with populated graphs. */
    void refreshForSnapshot();
    void tickForSnapshot();
    /** Visual-test only: deterministic art-directed graph data derived from the approved reference. */
    void setReferenceFixture (bool on);
    /** Renders only the canonical main view at 1536 x 1024 (times `scale`), without host chrome or the standalone strip. */
    juce::Image snapshotCanvas (float scale);
    juce::Component* mainView() const;

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
    /** Transparent layer under the Settings panel: an outside click only closes the panel (never reaches the control below). */
    struct OutsideClickCatcher : public juce::Component
    {
        std::function<void()> onClick;
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    } catcher;
    std::unique_ptr<SettingsPanel> settings;
    juce::Component::SafePointer<juce::Component> focusBeforeSettings;
    std::unique_ptr<juce::FileChooser> chooser;
    int frame = 0;
};
} // namespace pa
