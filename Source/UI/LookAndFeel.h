#pragma once

#include "Theme.h"

namespace pa
{
class PALookAndFeel : public juce::LookAndFeel_V4
{
public:
    PALookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::font (13.0f, 1); }
    juce::Font getPopupMenuFont() override { return theme::font (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return theme::font (12.5f, 1); }
    juce::Font getLabelFont (juce::Label&) override { return theme::font (14.0f); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
};

/** Paints a raised ivory card with soft shadow. */
void paintCard (juce::Graphics& g, juce::Rectangle<float> r, float radius = theme::cardRadius);
/** Paints a recessed dark display with subtle vertical shade and faint grid. */
void paintDisplay (juce::Graphics& g, juce::Rectangle<float> r, int vLines, int hLines);
} // namespace pa
