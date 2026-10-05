// Parameter-bound controls in the Playable Ambience style. All controls are real host parameters with gestures.
#pragma once

#include "LookAndFeel.h"
#include "Plugin/PluginProcessor.h"

namespace pa
{
/** Rotary knob: title above, cream knob, editable value field below. Can be re-bound to another parameter. */
class ParamKnob : public juce::Component
{
public:
    ParamKnob (PluginProcessor& p, int paramIndex, const juce::String& title, bool compact = false);
    void bind (int paramIndex, const juce::String& title);
    int boundParam() const noexcept { return index; }
    void resized() override;
    void paint (juce::Graphics&) override;
    void refreshValue();
    juce::Slider slider;

private:
    PluginProcessor& proc;
    int index = -1;
    juce::String titleText;
    bool compact = false;
    juce::Label value;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Segmented control for a choice/int/bool parameter, or free-standing with onSelect. */
class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (PluginProcessor* p, int paramIndex, juce::StringArray labels, std::vector<float> values = {});
    void setSelected (int idx, bool notify);
    int selected() const noexcept { return sel; }
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    std::function<void (int)> onSelect;
    void setFontHeight (float h) { fontH = h; repaint(); }

private:
    void choose (int i);
    PluginProcessor* proc;
    int index;
    juce::StringArray items;
    std::vector<float> vals;
    int sel = 0;
    float fontH = 12.5f;
    std::unique_ptr<juce::ParameterAttachment> attach;
};

/** Text toggle bound to a bool parameter (orange when on). */
class ParamToggleButton : public juce::TextButton
{
public:
    ParamToggleButton (PluginProcessor& p, int paramIndex, const juce::String& text);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attach;
};

/** ComboBox bound to a choice parameter. */
class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (PluginProcessor& p, int paramIndex);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attach;
};

/** Thin-stroke icon button. */
class IconButton : public juce::Button
{
public:
    enum class Icon { Folder, Save, Gear, Kebab, Heart, Prev, Next, Panic, TailKill, Play, Pause, Stop, Loop, Record, Export, Sliders, Close };
    IconButton (Icon i, const juce::String& tooltip);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void setFilled (bool f) { filled = f; repaint(); }

private:
    Icon icon;
    bool filled = false;
};

/** Small engine enable dot (orange = processing on). */
class EnableDot : public juce::Component, public juce::SettableTooltipClient
{
public:
    EnableDot (PluginProcessor& p, int paramIndex, const juce::String& what);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void refresh() { repaint(); }

private:
    void toggle();
    PluginProcessor& proc;
    int index;
};

/** [-] value [+] stepper for integer parameters. */
class ParamStepper : public juce::Component, public juce::SettableTooltipClient
{
public:
    ParamStepper (PluginProcessor& p, int paramIndex);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::String prefix;

private:
    void step (int d);
    PluginProcessor& proc;
    int index;
    juce::TextButton minus { "-" }, plus { "+" };
    std::unique_ptr<juce::ParameterAttachment> attach;
};

/** Stereo level meter pair (peak hold), fed from telemetry atomics. */
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (const juce::String& label) : name (label) {}
    void setLevels (float l, float r);
    void paint (juce::Graphics&) override;

private:
    juce::String name;
    float lv[2] { 0, 0 }, hold[2] { 0, 0 };
};

juce::String noteName (int midiNote, bool preferSharps, bool withOctave = true);
juce::String chordName (const std::vector<int>& notes);
bool keyPrefersSharps (int pitchClass);
} // namespace pa
