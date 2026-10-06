#include "LookAndFeel.h"

#include "BinaryData.h"

#include <map>

namespace pa
{
namespace theme
{
static juce::Typeface::Ptr face (int weight)
{
    static juce::Typeface::Ptr faces[6] = {
        juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, (size_t) BinaryData::InterRegular_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_ttf, (size_t) BinaryData::InterMedium_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, (size_t) BinaryData::InterSemiBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf, (size_t) BinaryData::InterBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::InterExtraBold_ttf, (size_t) BinaryData::InterExtraBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::InterLight_ttf, (size_t) BinaryData::InterLight_ttfSize),
    };
    return faces[juce::jlimit (0, 5, weight)];
}

juce::Font font (float height, int weight)
{
    return juce::Font (juce::FontOptions().withTypeface (face (weight)).withHeight (height));
}

juce::Font capFont (float capPx, int weight, float tracking)
{
    // Inter: cap height 1490/2048 em, ascent+descent (JUCE height) 2478/2048 em
    const float h = capPx * (2478.0f / 1490.0f);
    return juce::Font (juce::FontOptions().withTypeface (face (weight)).withHeight (h).withKerningFactor (tracking));
}

juce::Font spaced (float height, float tracking, int weight)
{
    return juce::Font (juce::FontOptions().withTypeface (face (weight)).withHeight (height).withKerningFactor (tracking));
}
} // namespace theme

// =============================================================================================== knob
juce::Image KnobRenderer::stationary (float R, float scale)
{
    struct Key { int r, s; bool operator< (const Key& o) const { return r != o.r ? r < o.r : s < o.s; } };
    static std::map<Key, juce::Image> cache;
    const Key key { (int) std::lround (R * 8.0f), (int) std::lround (scale * 100.0f) };
    if (auto it = cache.find (key); it != cache.end()) return it->second;

    const float half = R * 1.42f;                  // covers ticks and shadow
    const int px = (int) std::ceil (2.0f * half * scale);
    juce::Image img (juce::Image::ARGB, px, px, true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));
        const juce::Point<float> c (half, half);
        const auto disc = [&c] (float r) { return juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c); };

        // 1. contact + soft shadow (fixed light from the upper left)
        {
            juce::Path p; p.addEllipse (disc (R));
            juce::DropShadow (juce::Colour (0x783a2a18), (int) std::lround (R * 0.32f), { 1, (int) std::lround (R * 0.12f) }).drawForPath (g, p);
            juce::DropShadow (juce::Colour (0x40281a0c), (int) std::max (2L, std::lround (R * 0.08f)), { 0, (int) std::max (1L, std::lround (R * 0.04f)) }).drawForPath (g, p);
        }
        // 2. thin outline: warm light brown at the top, near black at the bottom
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffae9373), c.x, c.y - R, juce::Colour (0xff17110a), c.x, c.y + R, false));
        g.fillEllipse (disc (R + 1.5f));
        // 3. bevel ring: warm highlight upper left, darker lower right
        {
            juce::ColourGradient bev (juce::Colour (0xfffffbf2), c.x - R * 0.7f, c.y - R * 0.7f, juce::Colour (0xff9c9282), c.x + R * 0.6f, c.y + R * 0.75f, false);
            bev.addColour (0.55, juce::Colour (0xffe9e1d4));
            g.setGradientFill (bev);
            g.fillEllipse (disc (R - 0.6f));
        }
        // 4. ivory face: brightest upper left, deeper cream lower right
        {
            const float rf = R - 2.6f;
            juce::ColourGradient face (theme::knobFaceTop, c.x, c.y - rf, theme::knobFaceBottom, c.x, c.y + rf, false);
            face.addColour (0.45, juce::Colour (0xffeee8de));
            g.setGradientFill (face);
            g.fillEllipse (disc (rf));
            juce::ColourGradient hi (juce::Colours::white.withAlpha (0.55f), c.x - rf * 0.35f, c.y - rf * 0.42f,
                                     juce::Colours::white.withAlpha (0.0f), c.x - rf * 0.35f + rf * 0.95f, c.y - rf * 0.42f, true);
            g.setGradientFill (hi);
            g.fillEllipse (disc (rf));
        }
        // 6. nine small dark tick dots every 30 degrees across +/-120 degrees
        g.setColour (juce::Colour (0xff3a3833));
        const float tr = R * 1.235f, dr = std::max (1.1f, R * 0.047f);
        for (int i = 0; i < 9; ++i)
        {
            const float a = juce::degreesToRadians (-120.0f + 30.0f * (float) i);
            g.fillEllipse (juce::Rectangle<float> (2.0f * dr, 2.0f * dr).withCentre (c.getPointOnCircumference (tr, a)));
        }
    }
    cache[key] = img;
    return img;
}

void KnobRenderer::draw (juce::Graphics& g, juce::Point<float> centre, float R, float pos, bool enabled, bool hover, bool focused)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const float half = R * 1.42f;
    auto img = stationary (R, std::max (1.0f, scale));
    g.setOpacity (enabled ? 1.0f : 0.55f);
    g.drawImage (img, juce::Rectangle<float> (centre.x - half, centre.y - half, 2.0f * half, 2.0f * half), juce::RectanglePlacement::stretchToFit);
    g.setOpacity (1.0f);
    if (hover && enabled)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillEllipse (juce::Rectangle<float> (2.0f * (R - 2.6f), 2.0f * (R - 2.6f)).withCentre (centre));
    }
    pos = juce::jlimit (0.0f, 1.0f, pos);
    const float a0 = juce::degreesToRadians (kStart), a = juce::degreesToRadians (kStart + (kEnd - kStart) * pos);
    // 5. fine value arc on the rim (restrained: blends into the outline)
    if (pos > 0.002f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, R + 0.6f, R + 0.6f, 0.0f, a0, a, true);
        g.setColour ((enabled ? juce::Colour (0xffb5431e) : theme::textDisabled).withAlpha (0.88f));
        g.strokePath (arc, juce::PathStrokeType (std::max (1.6f, R * 0.075f), juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }
    // 7. orange radial marker from inside the face towards the rim
    {
        const auto p0 = centre.getPointOnCircumference (R * 0.40f, a), p1 = centre.getPointOnCircumference (R * 0.855f, a);
        juce::Path m; m.startNewSubPath (p0); m.lineTo (p1);
        const float w = std::max (2.0f, R * 0.104f);
        g.setColour (enabled ? theme::marker : theme::textDisabled);
        g.strokePath (m, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        // 8. faint light edge beside the marker
        const auto off = juce::Point<float> (std::cos (a), std::sin (a)) * (w * 0.55f);
        juce::Path e; e.startNewSubPath (p0 - off); e.lineTo (p1 - off);
        g.setColour (juce::Colours::white.withAlpha (enabled ? 0.28f : 0.1f));
        g.strokePath (e, juce::PathStrokeType (0.7f));
    }
    if (focused)
    {
        g.setColour (theme::accent.withAlpha (0.55f));
        g.drawEllipse (juce::Rectangle<float> (2.0f * (R + 3.5f), 2.0f * (R + 3.5f)).withCentre (centre), 1.0f);
    }
}

// =============================================================================================== look and feel
PALookAndFeel::PALookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::shell);
    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::TextButton::textColourOffId, theme::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::backgroundColourId, theme::fieldTop);
    setColour (juce::ComboBox::outlineColourId, theme::border);
    setColour (juce::ComboBox::arrowColourId, theme::text);
    setColour (juce::PopupMenu::backgroundColourId, theme::cardTop);
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffe8dfd1));
    setColour (juce::PopupMenu::highlightedTextColourId, theme::text);
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
    juce::Path p; p.addRoundedRectangle (r, radius);
    juce::DropShadow (theme::shadow, 6, { 0, 2 }).drawForPath (g, p);
    juce::DropShadow (juce::Colour (0x14301e0c), 2, { 0, 1 }).drawForPath (g, p);
    juce::ColourGradient base (theme::cardTop, r.getX(), r.getY(), theme::cardBottom, r.getX(), r.getBottom(), false);
    base.addColour (0.62, juce::Colour (0xffeee7dc));
    g.setGradientFill (base);
    g.fillPath (p);
    // faint radial lift towards the upper middle
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), r.getCentreX(), r.getY(), juce::Colours::white.withAlpha (0.0f),
                                             r.getCentreX(), r.getY() + r.getHeight() * 0.9f, true));
    g.fillPath (p);
    // delicate upper highlight and darker lower lip
    g.setColour (theme::cardEdgeLight.withAlpha (0.9f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
    g.setColour (juce::Colour (0x22806a50));
    g.strokePath (p, juce::PathStrokeType (1.0f));
    g.setColour (theme::cardEdgeDark);
    g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + radius, r.getRight() - radius);
}

void paintDisplay (juce::Graphics& g, juce::Rectangle<float> r, int vLines, int hLines)
{
    juce::Path p; p.addRoundedRectangle (r, 10.0f);
    g.setGradientFill (juce::ColourGradient (theme::displayTop, r.getX(), r.getY(), theme::display, r.getX(), r.getBottom(), false));
    g.fillPath (p);
    g.saveState();
    g.reduceClipRegion (p);
    // darkened corners
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(), juce::Colours::black.withAlpha (0.22f),
                                             r.getX(), r.getY(), true));
    g.fillRect (r);
    g.setColour (theme::displayGrid);
    for (int i = 1; i < vLines; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / (float) vLines), r.getY(), r.getBottom());
    for (int i = 1; i < hLines; ++i) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / (float) hLines), r.getX(), r.getRight());
    g.restoreState();
    // recessed edge: dark upper rim, faint light lower rim
    g.setColour (juce::Colour (0x660c1010));
    g.strokePath (p, juce::PathStrokeType (1.0f));
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawHorizontalLine ((int) r.getBottom(), r.getX() + 10.0f, r.getRight() - 10.0f);
}

void paintField (juce::Graphics& g, juce::Rectangle<float> r, float radius, bool raised, bool hover)
{
    juce::Path p; p.addRoundedRectangle (r, radius);
    if (raised) juce::DropShadow (juce::Colour (0x18301e0c), 3, { 0, 1 }).drawForPath (g, p);
    g.setGradientFill (juce::ColourGradient (raised ? juce::Colour (0xfff7f3ec) : theme::fieldTop, r.getX(), r.getY(),
                                             raised ? juce::Colour (0xffebe5da) : theme::fieldBottom, r.getX(), r.getBottom(), false));
    g.fillPath (p);
    if (hover) { g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillPath (p); }
    g.setColour (raised ? juce::Colour (0xffcdc4b5) : juce::Colour (0xffc6bdad));
    g.strokePath (p, juce::PathStrokeType (1.0f));
    if (raised)
    {
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
    }
}

void paintActive (juce::Graphics& g, juce::Rectangle<float> r, float radius, bool hover)
{
    juce::Path p; p.addRoundedRectangle (r, radius);
    juce::DropShadow (juce::Colour (0x40a03008), 4, { 0, 2 }).drawForPath (g, p);
    g.setGradientFill (juce::ColourGradient (hover ? theme::accentTop.brighter (0.06f) : theme::accentTop, r.getX(), r.getY(), theme::accentBottom, r.getX(), r.getBottom(), false));
    g.fillPath (p);
    g.setColour (juce::Colour (0x80ffb08a));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
    g.setColour (juce::Colour (0x50a0300a));
    g.strokePath (p, juce::PathStrokeType (0.8f));
}

void PALookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s)
{
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float R = (float) s.getProperties().getWithDefault ("faceRadius", std::min (w, h) * 0.5f / 1.42f);
    KnobRenderer::draw (g, area.getCentre(), R, pos, s.isEnabled(), s.isMouseOverOrDragging(), s.hasKeyboardFocus (false));
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
    const float rad = (float) b.getProperties().getWithDefault ("radius", std::min (6.0f, r.getHeight() * 0.3f));
    if (b.getToggleState()) paintActive (g, r, rad, highlighted);
    else paintField (g, r, rad, true, highlighted && b.isEnabled());
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.07f)); g.fillRoundedRectangle (r, rad); }
    if (b.hasKeyboardFocus (false)) { g.setColour (theme::accent.withAlpha (0.8f)); g.drawRoundedRectangle (r.reduced (1.5f), rad, 1.2f); }
}

void PALookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const float cap = (float) b.getProperties().getWithDefault ("capPx", std::min (10.0f, b.getHeight() * 0.3f));
    g.setFont (theme::capFont (cap, 1));
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

void PALookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& c)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    paintField (g, r, std::min (6.0f, (float) h * 0.2f), true, c.isMouseOver (true) && c.isEnabled());
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.05f)); g.fillRoundedRectangle (r, 6.0f); }
    if (c.hasKeyboardFocus (true)) { g.setColour (theme::accent.withAlpha (0.8f)); g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, 1.2f); }
    // thin custom chevron
    const float cx = (float) w - (w < 110 ? 15.0f : 25.0f), cy = (float) h * 0.5f, s = std::min (6.0f, (float) h * 0.17f);
    juce::Path ch;
    ch.startNewSubPath (cx - s, cy - s * 0.45f); ch.lineTo (cx, cy + s * 0.55f); ch.lineTo (cx + s, cy - s * 0.45f);
    g.setColour (c.isEnabled() ? theme::text : theme::textDisabled);
    g.strokePath (ch, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void PALookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    const bool narrow = box.getWidth() < 110;
    const int inset = (int) box.getProperties().getWithDefault ("textInset", narrow ? 9 : 17);
    label.setBounds (inset - 3, 1, box.getWidth() - inset - (narrow ? 22 : 36), box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void PALookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setGradientFill (juce::ColourGradient (theme::cardTop, 0, 0, theme::cardBottom, 0, (float) h, false));
    g.fillRect (r);
    g.setColour (theme::border);
    g.drawRect (r, 1.0f);
}

void PALookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& idealWidth, int& idealHeight)
{
    if (isSeparator) { idealWidth = 50; idealHeight = 9; return; }
    juce::ignoreUnused (standardMenuItemHeight);
    idealHeight = 28;
    idealWidth = (int) juce::GlyphArrangement::getStringWidth (getPopupMenuFont(), text) + 56;
}

void PALookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                       bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText, const juce::Drawable*, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (theme::border);
        g.drawHorizontalLine (area.getCentreY(), (float) area.getX() + 10.0f, (float) area.getRight() - 10.0f);
        return;
    }
    auto r = area.reduced (3, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (juce::Colour (0xffe6dccd));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
    }
    if (isTicked)
    {
        juce::Path tick;
        const float x = (float) r.getX() + 10.0f, y = (float) r.getCentreY();
        tick.startNewSubPath (x, y); tick.lineTo (x + 4.0f, y + 4.0f); tick.lineTo (x + 11.0f, y - 5.0f);
        g.setColour (theme::accent);
        g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (textColour != nullptr ? *textColour : (isActive ? theme::text : theme::textDisabled));
    g.setFont (getPopupMenuFont());
    g.drawFittedText (text, r.withTrimmedLeft (28).withTrimmedRight (hasSubMenu ? 24 : 8), juce::Justification::centredLeft, 1);
    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (theme::textMuted);
        g.drawText (shortcutKeyText, r.withTrimmedRight (10), juce::Justification::centredRight);
    }
    if (hasSubMenu)
    {
        juce::Path ch;
        const float cx = (float) r.getRight() - 14.0f, cy = (float) r.getCentreY();
        ch.startNewSubPath (cx - 2.5f, cy - 5.0f); ch.lineTo (cx + 2.5f, cy); ch.lineTo (cx - 2.5f, cy + 5.0f);
        g.setColour (theme::textMuted);
        g.strokePath (ch, juce::PathStrokeType (1.5f));
    }
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
