// Advanced drawer: every remaining parameter with help text, method-aware greying, A/B tools and diagnostics.
#pragma once

#include "Widgets.h"

namespace pa
{
class AdvancedPanel : public juce::Component, private juce::Timer
{
public:
    AdvancedPanel (PluginProcessor& p, std::function<void()> onClose);
    ~AdvancedPanel() override;
    void showTab (const juce::String& name);
    juce::String currentTab() const { return tabNames[tabs.selected()]; }
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    std::function<void()> onSaveExperiment;

private:
    struct Row;
    struct Content;
    void timerCallback() override;
    void build (int tab);
    void addParamRow (int index, std::function<bool()> enabled = nullptr, const juce::String& disabledWhy = {});
    void addSection (const juce::String& title, const juce::String& note, std::function<bool()> enabled = nullptr, const juce::String& disabledWhy = {});
    void addButtons (std::vector<std::pair<juce::String, std::function<void()>>> buttons);
    void addInfo (std::function<juce::String()> textFn);
    void refreshDiagnostics();

    PluginProcessor& proc;
    std::function<void()> closeFn;
    juce::StringArray tabNames { "Harmony", "Delay", "Reverb", "MIDI", "Mix / Timing", "Diagnostics" };
    Segmented tabs;
    IconButton closeButton { IconButton::Icon::Close, "Close (Esc)" };
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
    juce::TextEditor diagText;
    std::function<bool()> sectionEnabled;
    juce::String sectionWhy;
};
} // namespace pa
