// Contextual effect settings (shown in place of an effect's graph) and the compact plugin-wide Settings panel.
// Every control attaches to the existing parameters (stable ids, host gestures, automation, presets). Opening or
// closing a view changes nothing in the sound; which view is open is editor-only state.
#pragma once

#include "Widgets.h"

namespace pa
{
/** One labelled secondary control: label and value on the first line, compact slider / segmented / stepper / dropdown below. */
class DetailControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Style { Auto, Slider, Segments, Combo, Stepper };
    DetailControl (PluginProcessor& p, int paramIndex, const juce::String& label, const juce::String& help,
                   Style style = Style::Auto, juce::StringArray segmentLabels = {});
    /** Free-standing cell with custom content (e.g. snapshot buttons, tap offsets, status text). */
    DetailControl (const juce::String& label, const juce::String& help, std::unique_ptr<juce::Component> content);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
    juce::String help;
    std::function<bool()> enabledWhen;

private:
    PluginProcessor* proc = nullptr;
    int index = -1;
    juce::String label;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
    std::unique_ptr<Segmented> segments;
    std::unique_ptr<ParamCombo> combo;
    std::unique_ptr<ParamStepper> stepper;
    std::unique_ptr<juce::Label> value;
    std::unique_ptr<juce::Component> custom;
};

/** In-place settings surface for one effect: same bounds as the graph it replaces. Pages depend on the current mode. */
class EffectDetail : public juce::Component
{
public:
    enum class Effect { Delay, Reverb, Harmony };
    EffectDetail (PluginProcessor& p, Effect e);
    void update();                         // ~30 Hz while visible: rebuild on mode/source change, refresh values, help line
    void showPage (int page);
    void resized() override;
    void paint (juce::Graphics&) override;
    std::function<void()> onBack;
    juce::String titleText() const { return title; }

private:
    struct Page { juce::String name; int columns = 2; std::vector<std::unique_ptr<DetailControl>> items; };
    int modeKey() const;
    void rebuild();
    void addPageControls (Page& pg);
    PluginProcessor& proc;
    Effect effect;
    juce::String title;
    std::vector<Page> pages;
    int current = 0, builtKey = -1;
    std::unique_ptr<Segmented> pageTabs;
    std::unique_ptr<juce::Component> headingExtra;
    juce::TextButton helpBtn { "?" };
    struct BackButton : public juce::Button { BackButton() : juce::Button ("Back to graph") {} void paintButton (juce::Graphics&, bool, bool) override; } backBtn;
    juce::String helpLine;
};

/** Compact plugin-wide Settings: MIDI / Setup, Audio, Support. Anchored near the header gear, inside the editor. */
class SettingsPanel : public juce::Component, private juce::Timer
{
public:
    SettingsPanel (PluginProcessor& p, std::function<void()> onClose, std::function<void()> onSaveExperiment);
    ~SettingsPanel() override;
    void showPage (int page);
    int page() const { return current; }
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int kW = 470, kH = 480;

private:
    void timerCallback() override;
    void build();
    PluginProcessor& proc;
    std::function<void()> closeFn, saveExperimentFn;
    Segmented tabs { nullptr, -1, { "MIDI / SETUP", "AUDIO", "SUPPORT" } };
    IconButton closeBtn { IconButton::Icon::Close, "Close settings (Esc)" };
    int current = 0;
    std::vector<std::unique_ptr<DetailControl>> items;
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::Label> status;
    std::unique_ptr<juce::TextEditor> diag;
    juce::Rectangle<int> buttonRow;
};
} // namespace pa
