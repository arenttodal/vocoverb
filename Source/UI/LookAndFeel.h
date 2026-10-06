#pragma once

#include "Theme.h"

namespace pa
{
/** The one calibrated rotary renderer used by every knob (engine/harmony 68 px face, header Dry/Wet ~58 px face).
    Stationary layers (contact shadow, outline, bevel, face gradient, tick dots) are cached per size and device scale;
    only the value arc and the marker are drawn per paint, so the light source never rotates with the value. */
struct KnobRenderer
{
    static constexpr float kStart = -135.0f, kEnd = 135.0f; // degrees from 12 o'clock (270 degree sweep)
    /** centre and face radius in the caller's coordinates; pos 0..1. */
    static void draw (juce::Graphics& g, juce::Point<float> centre, float faceRadius, float pos, bool enabled, bool hover, bool focused);

private:
    static juce::Image stationary (float faceRadius, float scale);
};

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
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return theme::capFont (b.getHeight() < 31 ? 8.6f : 10.4f, 1); }
    juce::Font getPopupMenuFont() override { return theme::font (14.5f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return theme::capFont (10.0f, 1); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                            bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour* textColour) override;
    int getPopupMenuBorderSize() override { return 5; }
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& idealWidth, int& idealHeight) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
};

/** Raised ivory card: restrained warm shadow, base gradient, fine upper highlight and darker lower lip. */
void paintCard (juce::Graphics& g, juce::Rectangle<float> r, float radius = theme::cardRadius);
/** Recessed dark well with grid (vertical/horizontal line positions in local x / y). */
void paintDisplay (juce::Graphics& g, juce::Rectangle<float> r, int vLines, int hLines);
/** Warm field / neutral button surface (value readouts, dropdowns, unselected buttons). */
void paintField (juce::Graphics& g, juce::Rectangle<float> r, float radius, bool raised, bool hover = false);
/** Selected orange button surface. */
void paintActive (juce::Graphics& g, juce::Rectangle<float> r, float radius, bool hover = false);
} // namespace pa
