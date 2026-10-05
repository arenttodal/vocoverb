#include "LookAndFeel.h"

#include "BinaryData.h"

namespace pa
{
namespace theme
{
static juce::Typeface::Ptr face (int weight)
{
    static juce::Typeface::Ptr faces[4] = {
        juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, (size_t) BinaryData::InterRegular_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_ttf, (size_t) BinaryData::InterMedium_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, (size_t) BinaryData::InterSemiBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf, (size_t) BinaryData::InterBold_ttfSize),
    };
    return faces[juce::jlimit (0, 3, weight)];
}

juce::Font font (float height, int weight)
{
    return juce::Font (juce::FontOptions().withTypeface (face (weight)).withHeight (height));
}

juce::Font spaced (float height, float tracking, int weight)
{
    return juce::Font (juce::FontOptions().withTypeface (face (weight)).withHeight (height).withKerningFactor (tracking));
}
} // namespace theme

PALookAndFeel::PALookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::shell);
    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::TextButton::textColourOffId, theme::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::backgroundColourId, theme::valueBox);
    setColour (juce::ComboBox::outlineColourId, theme::border);
    setColour (juce::ComboBox::arrowColourId, theme::text);
    setColour (juce::PopupMenu::backgroundColourId, theme::cardTop);
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, theme::textMuted);
    setColour (juce::TextEditor::textColourId, theme::text);
    setColour (juce::TextEditor::backgroundColourId, theme::valueBox);
    setColour (juce::TextEditor::highlightColourId, theme::accent.withAlpha (0.3f));
    setColour (juce::TextEditor::outlineColourId, theme::border);
    setColour (juce::TextEditor::focusedOutlineColourId, theme::accent);
    setColour (juce::CaretComponent::caretColourId, theme::text);
    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::ScrollBar::thumbColourId, theme::borderDark);
    setColour (juce::AlertWindow::backgroundColourId, theme::cardTop);
    setColour (juce::AlertWindow::textColourId, theme::text);
    setColour (juce::AlertWindow::outlineColourId, theme::border);
    setColour (juce::TooltipWindow::backgroundColourId, theme::display);
    setColour (juce::TooltipWindow::textColourId, theme::ivoryTrace);
    setColour (juce::ListBox::backgroundColourId, theme::cardTop);
    setColour (juce::DialogWindow::backgroundColourId, theme::cardTop);
    setDefaultSansSerifTypeface (theme::font (14.0f).getTypefacePtr());
}

void paintCard (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    // soft, short shadow
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.025f * (float) i));
        g.fillRoundedRectangle (r.translated (0.0f, (float) i * 0.8f).expanded ((float) (4 - i) * 0.6f), radius + 1.0f);
    }
    g.setGradientFill (juce::ColourGradient (theme::cardTop, r.getX(), r.getY(), theme::cardBottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::white.withAlpha (0.65f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (theme::border.withAlpha (0.55f));
    g.drawRoundedRectangle (r.reduced (0.0f), radius, 0.8f);
}

void paintDisplay (juce::Graphics& g, juce::Rectangle<float> r, int vLines, int hLines)
{
    g.setGradientFill (juce::ColourGradient (theme::displayTop, r.getX(), r.getY(), theme::display, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (theme::displayGrid);
    for (int i = 1; i < vLines; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) vLines;
        g.drawVerticalLine ((int) x, r.getY() + 4.0f, r.getBottom() - 4.0f);
    }
    for (int i = 1; i < hLines; ++i)
    {
        const float y = r.getY() + r.getHeight() * (float) i / (float) hLines;
        g.drawHorizontalLine ((int) y, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
}

void PALookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = std::min (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float rOuter = size * 0.5f - 2.0f;
    const float rFace = rOuter * 0.78f;
    const bool enabled = s.isEnabled();

    // dotted scale
    const int dots = 11;
    for (int i = 0; i < dots; ++i)
    {
        const float a = start + (end - start) * (float) i / (float) (dots - 1);
        const float dr = (i == 0 || i == dots - 1 || i == (dots - 1) / 2) ? 1.6f : 1.1f;
        const auto pt = centre.getPointOnCircumference (rOuter - 1.0f, a);
        g.setColour (theme::text.withAlpha (enabled ? 0.55f : 0.25f));
        g.fillEllipse (pt.x - dr, pt.y - dr, dr * 2.0f, dr * 2.0f);
    }
    // shadow
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    g.fillEllipse (juce::Rectangle<float> (rFace * 2.0f, rFace * 2.0f).withCentre (centre.translated (0.0f, 2.2f)).expanded (0.8f));
    // value arc (subtle orange)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, rFace + 2.3f, rFace + 2.3f, 0.0f, start, start + (end - start) * pos, true);
        g.setColour (theme::accent.withAlpha (enabled ? 0.55f : 0.2f));
        g.strokePath (arc, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // face
    const auto face = juce::Rectangle<float> (rFace * 2.0f, rFace * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (theme::knobFaceTop, face.getX(), face.getY(), theme::knobFaceBottom, face.getX(), face.getBottom(), false));
    g.fillEllipse (face);
    g.setColour (theme::text.withAlpha (0.82f));
    g.drawEllipse (face, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.drawEllipse (face.reduced (1.6f), 0.8f);
    // indicator
    const float a = start + (end - start) * pos;
    const auto p1 = centre.getPointOnCircumference (rFace * 0.42f, a);
    const auto p2 = centre.getPointOnCircumference (rFace * 0.9f, a);
    g.setColour (enabled ? theme::accent : theme::textDisabled);
    g.drawLine (juce::Line<float> (p1, p2), 3.0f);
    if (s.hasKeyboardFocus (true))
    {
        g.setColour (theme::accent.withAlpha (0.6f));
        g.drawEllipse (face.expanded (4.0f), 1.0f);
    }
}

void PALookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s); return; }
    const float cy = (float) y + (float) h * 0.5f;
    const auto track = juce::Rectangle<float> ((float) x, cy - 2.0f, (float) w, 4.0f);
    g.setColour (theme::border);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (s.isEnabled() ? theme::accent : theme::textDisabled);
    g.fillRoundedRectangle (track.withRight (pos), 2.0f);
    const auto thumb = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ pos, cy });
    g.setColour (juce::Colours::black.withAlpha (0.15f));
    g.fillEllipse (thumb.translated (0, 1.2f));
    g.setGradientFill (juce::ColourGradient (theme::knobFaceTop, thumb.getX(), thumb.getY(), theme::knobFaceBottom, thumb.getX(), thumb.getBottom(), false));
    g.fillEllipse (thumb);
    g.setColour (theme::text.withAlpha (0.7f));
    g.drawEllipse (thumb, 1.0f);
    if (s.hasKeyboardFocus (true)) { g.setColour (theme::accent); g.drawEllipse (thumb.expanded (2.5f), 1.0f); }
}

void PALookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const float rad = std::min (6.0f, r.getHeight() * 0.3f);
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (theme::accentTop, r.getX(), r.getY(), theme::accentBottom, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (theme::accentBottom.darker (0.25f));
        g.drawRoundedRectangle (r, rad, 0.8f);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (theme::cardTop.brighter (0.02f), r.getX(), r.getY(), theme::cardBottom, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (highlighted ? theme::borderDark : theme::border);
        g.drawRoundedRectangle (r, rad, 0.9f);
    }
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.08f)); g.fillRoundedRectangle (r, rad); }
    else if (highlighted && ! on) { g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillRoundedRectangle (r, rad); }
    if (b.hasKeyboardFocus (false)) { g.setColour (theme::accent.withAlpha (0.8f)); g.drawRoundedRectangle (r.reduced (1.5f), rad, 1.2f); }
}

void PALookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (theme::font (std::min (13.0f, b.getHeight() * 0.42f), 1));
    const bool on = b.getToggleState();
    g.setColour (! b.isEnabled() ? theme::textDisabled : (on ? juce::Colours::white : theme::text));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 1), juce::Justification::centred, 1, 0.8f);
}

void PALookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    const float h = std::min (18.0f, r.getHeight() - 4.0f);
    const auto sw = juce::Rectangle<float> (h * 1.8f, h).withY (r.getCentreY() - h * 0.5f).withX (r.getX() + 2.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? theme::accent : theme::border);
    g.fillRoundedRectangle (sw, h * 0.5f);
    const auto knob = juce::Rectangle<float> (h - 4.0f, h - 4.0f).withCentre ({ on ? sw.getRight() - h * 0.5f : sw.getX() + h * 0.5f, sw.getCentreY() });
    g.setColour (theme::knobFaceTop);
    g.fillEllipse (knob);
    g.setColour (theme::text.withAlpha (0.4f));
    g.drawEllipse (knob, 0.8f);
    if (highlighted || b.hasKeyboardFocus (false)) { g.setColour (theme::accent.withAlpha (0.5f)); g.drawRoundedRectangle (sw.expanded (1.5f), h * 0.5f + 1.5f, 1.0f); }
    g.setColour (b.isEnabled() ? theme::text : theme::textDisabled);
    g.setFont (theme::font (13.0f));
    g.drawFittedText (b.getButtonText(), r.withLeft (sw.getRight() + 8.0f).toNearestInt(), juce::Justification::centredLeft, 1);
}

void PALookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& c)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (theme::valueBox);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (c.hasKeyboardFocus (true) ? theme::accent : theme::border);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (c.isEnabled() ? theme::text : theme::textDisabled);
    g.fillPath (arrow);
}

void PALookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (theme::cardTop);
    g.setColour (theme::border);
    g.drawRect (0, 0, w, h, 1);
}

void PALookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (theme::display);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (theme::ivoryTrace);
    g.setFont (theme::font (13.0f));
    g.drawFittedText (text, r.reduced (8.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 6);
}

juce::Rectangle<int> PALookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const int w = std::min (380, (int) juce::GlyphArrangement::getStringWidth (theme::font (13.0f), tipText) + 24);
    const int lines = 1 + (int) juce::GlyphArrangement::getStringWidth (theme::font (13.0f), tipText) / 340;
    const int h = 10 + 17 * lines;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

void PALookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool)
{
    const auto thumb = vertical ? juce::Rectangle<int> (x + 2, thumbStart, w - 4, thumbSize) : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, h - 4);
    g.setColour (over ? theme::borderDark : theme::border);
    g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
}

void PALookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (theme::valueBox);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 4.0f);
}

void PALookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? theme::accent : theme::border);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 4.0f, 1.0f);
}
} // namespace pa
