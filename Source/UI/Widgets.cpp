#include "Widgets.h"

namespace pa
{
// ============================================================================ helpers
bool keyPrefersSharps (int pc)
{
    pc = ((pc % 12) + 12) % 12;
    return pc == 7 || pc == 2 || pc == 9 || pc == 4 || pc == 11 || pc == 6; // G D A E B F#
}

juce::String noteName (int n, bool sharps, bool withOctave)
{
    static const char* sh[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* fl[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    if (n < 0) return "-";
    juce::String s = (sharps ? sh : fl)[n % 12];
    s = s.replace ("b", juce::CharPointer_UTF8 ("\xe2\x99\xad")).replace ("#", juce::CharPointer_UTF8 ("\xe2\x99\xaf"));
    if (withOctave) s << (n / 12 - 1);
    return s;
}

juce::String chordName (const std::vector<int>& notes)
{
    if (notes.empty()) return {};
    int mask = 0;
    for (int n : notes) mask |= 1 << (n % 12);
    struct T { int iv; const char* name; };
    static const T tmpl[] = { { 0x091, "MAJOR" }, { 0x089, "MINOR" }, { 0x085, "SUS2" }, { 0x0a1, "SUS4" }, { 0x081, "5" },
                              { 0x891, "MAJ7" }, { 0x489, "MIN7" }, { 0x491, "7" }, { 0x095, "ADD9" }, { 0x049, "DIM" }, { 0x111, "AUG" } };
    const int lowest = *std::min_element (notes.begin(), notes.end()) % 12;
    // prefer the bass note as root, then any
    for (int pass = 0; pass < 2; ++pass)
        for (int r = 0; r < 12; ++r)
        {
            const int root = pass == 0 ? lowest : r;
            const int rot = ((mask >> root) | (mask << (12 - root))) & 0xfff;
            for (const auto& t : tmpl)
                if (rot == t.iv)
                {
                    const bool sharps = keyPrefersSharps (root);
                    return noteName (root, sharps, false).toUpperCase() + " " + t.name;
                }
            if (pass == 0) break;
        }
    if (notes.size() == 1) return noteName (notes[0], keyPrefersSharps (notes[0]), true).toUpperCase();
    juce::StringArray s;
    for (int n : notes) s.add (noteName (n, false, false));
    return s.joinIntoString (" ");
}

// ============================================================================ ParamKnob
ParamKnob::ParamKnob (PluginProcessor& p, int paramIndex, const juce::String& title, bool compactStyle)
    : ParamKnob (p, paramIndex, title, compactStyle ? Style::Compact : Style::Engine) {}

ParamKnob::ParamKnob (PluginProcessor& p, int paramIndex, const juce::String& title, Style st) : proc (p), compact (st == Style::Compact), style (st)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setMouseDragSensitivity (220);
    // Shift-drag swaps to fine velocity mode
    slider.setVelocityModeParameters (0.35, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    slider.setWantsKeyboardFocus (true);
    slider.getProperties().set ("faceRadius", style == Style::Header ? 22.0f : 34.0f);
    addAndMakeVisible (slider);
    value.setJustificationType (juce::Justification::centred);
    value.setEditable (false, true, false);
    value.setFont (style == Style::Header ? theme::capFont (10.5f * theme::kCapPerEm, 0) : theme::capFont (11.2f, 0));
    value.setColour (juce::Label::textColourId, theme::text);
    value.setColour (juce::Label::textWhenEditingColourId, theme::text);
    value.setColour (juce::Label::backgroundWhenEditingColourId, theme::valueBox);
    value.setColour (juce::Label::outlineWhenEditingColourId, theme::accent);
    value.setBorderSize ({ 0, 0, 0, 0 });
    value.onTextChange = [this] {
        if (auto* prm = proc.param (index))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getValueForText (value.getText()));
            prm->endChangeGesture();
        }
        refreshValue();
    };
    addAndMakeVisible (value);
    slider.onValueChange = [this] { refreshValue(); };
    bind (paramIndex, title);
}

void ParamKnob::bind (int paramIndex, const juce::String& title)
{
    if (paramIndex == index && title == titleText) return;
    attachment.reset();
    index = paramIndex;
    titleText = title;
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramInfo (index).id, slider);
    if (auto* prm = proc.param (index))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    slider.setTitle (title.isNotEmpty() ? title : juce::String (paramInfo (index).name));
    slider.setDescription (paramInfo (index).help);
    slider.setTooltip (juce::String (paramInfo (index).help) + "  (double-click: default, shift-drag: fine)");
    value.setTooltip ("Double-click to type a value");
    value.setTitle (juce::String (paramInfo (index).name) + " value");
    refreshValue();
    repaint();
}

void ParamKnob::refreshValue()
{
    if (auto* prm = proc.param (index))
    {
        value.setText (prm->getCurrentValueAsText(), juce::dontSendNotification);
        if (compact) slider.setTooltip (juce::String (paramInfo (index).help) + "  Now: " + prm->getCurrentValueAsText());
    }
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    if (compact)
    {
        value.setVisible (false);
        r.removeFromBottom (14);
        slider.setBounds (r.reduced (1));
        return;
    }
    if (style == Style::Header)
    {
        // drag region a little beyond the 54 px tick ring; captions: ring + 4 px, label, + 3 px, value
        slider.setBounds (juce::Rectangle<int> (62, 58).withCentre ({ kHeaderCx, kHeaderCy }));
        value.setBounds (juce::Rectangle<int> (80, 14).withCentre ({ kHeaderCx, kHeaderCy + 27 + 4 + 8 + 3 + 4 }));
        return;
    }
    // canonical engine geometry: label cap top at y 7, knob centre (70, 64), value field 93 x 30 at y 112
    const int side = 96;
    slider.setBounds (juce::Rectangle<int> (side, side).withCentre ({ kEngineCx, kEngineCy }));
    const int dy = (int) getProperties().getWithDefault ("boxDy", 0);
    value.setBounds (juce::Rectangle<int> (kEngineCx - 46, 113 + dy, 93, 28));
}

void ParamKnob::paint (juce::Graphics& g)
{
    if (compact)
    {
        g.setColour (theme::text);
        g.setFont (theme::font (11.0f, 1));
        g.drawText (titleText.toUpperCase(), getLocalBounds().removeFromBottom (14), juce::Justification::centred);
        return;
    }
    const auto col = isEnabled() ? theme::textLabel : theme::textDisabled;
    if (style == Style::Header)
    {
        g.setColour (col);
        const float cap = 10.5f * theme::kCapPerEm;
        g.setFont (theme::capFont (cap, 1, 0.02f));
        const float capTop = (float) kHeaderCy + 27.0f + 4.0f;
        g.drawText (titleText, juce::Rectangle<float> (0.0f, capTop - (cap * 1.663f - cap) * 0.5f, (float) getWidth(), cap * 1.663f), juce::Justification::centred, false);
        return;
    }
    g.setColour (isEnabled() ? theme::text : theme::textDisabled);
    g.setFont (theme::capFont (9.4f, 2, 0.0f));
    g.drawText (titleText.toUpperCase(), juce::Rectangle<float> (0.0f, 3.0f, (float) getWidth(), 14.0f), juce::Justification::centred, false);
    auto box = juce::Rectangle<float> ((float) kEngineCx - 46.5f, 112.0f + (float) (int) getProperties().getWithDefault ("boxDy", 0), 93.0f, 30.0f);
    paintField (g, box, 5.0f, false);
}

// ============================================================================ Segmented
Segmented::Segmented (PluginProcessor* p, int paramIndex, juce::StringArray labels, std::vector<float> values)
    : proc (p), index (paramIndex), items (std::move (labels)), vals (std::move (values))
{
    if (vals.empty()) for (int i = 0; i < items.size(); ++i) vals.push_back ((float) i);
    setWantsKeyboardFocus (true);
    if (proc != nullptr && index >= 0)
    {
        attach = std::make_unique<juce::ParameterAttachment> (*proc->param (index), [this] (float v) {
            for (int i = 0; i < (int) vals.size(); ++i) if (std::abs (vals[(size_t) i] - v) < 0.5f) { sel = i; repaint(); return; }
        });
        attach->sendInitialUpdate();
        setTitle (paramInfo (index).name);
        setTooltip (paramInfo (index).help);
    }
}

void Segmented::setSelected (int idx, bool notify)
{
    sel = juce::jlimit (0, items.size() - 1, idx);
    repaint();
    if (notify && onSelect) onSelect (sel);
}

void Segmented::choose (int i)
{
    i = juce::jlimit (0, items.size() - 1, i);
    sel = i;
    if (attach) attach->setValueAsCompleteGesture (vals[(size_t) i]);
    if (onSelect) onSelect (i);
    repaint();
}

void Segmented::resized() {}

void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = (float) getProperties().getWithDefault ("radius", 6.0f);
    paintField (g, r, rad, true);
    const int n = items.size();
    const float w = r.getWidth() / (float) n;
    const float first = n == 2 ? r.getWidth() * (float) getProperties().getWithDefault ("firstFraction", 0.5) : w;
    for (int i = 0; i < n; ++i)
    {
        auto cell = n == 2 ? (i == 0 ? r.withWidth (first) : r.withTrimmedLeft (first))
                           : juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
        if (i == sel)
        {
            juce::Path p;
            p.addRoundedRectangle (cell.getX(), cell.getY(), cell.getWidth(), cell.getHeight(), rad, rad, i == 0, i == n - 1, i == 0, i == n - 1);
            g.setGradientFill (juce::ColourGradient (isEnabled() ? theme::accentTop : theme::border, cell.getX(), cell.getY(),
                                                     isEnabled() ? theme::accentBottom : theme::borderDark, cell.getX(), cell.getBottom(), false));
            g.fillPath (p);
            g.setColour (juce::Colour (0x70ffb08a));
            g.drawHorizontalLine ((int) cell.getY() + 1, cell.getX() + (i == 0 ? rad : 0.0f), cell.getRight() - (i == n - 1 ? rad : 0.0f));
        }
        else if (i > 0 && i != sel + 1)
        {
            g.setColour (theme::border);
            g.drawVerticalLine ((int) cell.getX(), cell.getY() + 1.0f, cell.getBottom() - 1.0f);
        }
        g.setColour (i == sel ? juce::Colours::white : (isEnabled() ? theme::text : theme::textDisabled));
        g.setFont (theme::capFont (fontH, 1));
        g.drawFittedText (items[i], cell.reduced (3.0f, 1.0f).toNearestInt(), juce::Justification::centred, 1, 0.75f);
    }
    if (hasKeyboardFocus (false)) { g.setColour (theme::accent); g.drawRoundedRectangle (r, rad, 1.0f); }
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled()) return;
    if (items.size() == 2) { choose ((float) e.x < (float) getWidth() * (float) getProperties().getWithDefault ("firstFraction", 0.5) ? 0 : 1); return; }
    const int i = (int) ((float) e.x / (float) getWidth() * (float) items.size());
    choose (i);
}

bool Segmented::keyPressed (const juce::KeyPress& k)
{
    if (k.isKeyCode (juce::KeyPress::leftKey)) { choose (sel - 1); return true; }
    if (k.isKeyCode (juce::KeyPress::rightKey)) { choose (sel + 1); return true; }
    return false;
}

// ============================================================================ toggles / combos
ParamToggleButton::ParamToggleButton (PluginProcessor& p, int paramIndex, const juce::String& text) : juce::TextButton (text)
{
    setClickingTogglesState (true);
    attach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, paramInfo (paramIndex).id, *this);
    setTooltip (paramInfo (paramIndex).help);
    setTitle (paramInfo (paramIndex).name);
}

ParamCombo::ParamCombo (PluginProcessor& p, int paramIndex)
{
    juce::StringArray ch;
    for (auto& c : paramChoices (paramIndex)) ch.add (c);
    addItemList (ch, 1);
    attach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, paramInfo (paramIndex).id, *this);
    setTooltip (paramInfo (paramIndex).help);
    setTitle (paramInfo (paramIndex).name);
}

// ============================================================================ IconButton
IconButton::IconButton (Icon i, const juce::String& tip) : juce::Button (tip), icon (i)
{
    setTooltip (tip);
    setTitle (tip);
    setWantsKeyboardFocus (true);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const float s = std::min ((float) getProperties().getWithDefault ("iconSize", 1000.0f), std::min (r.getWidth(), r.getHeight()));
    auto b = r.withSizeKeepingCentre (s, s);
    const juce::Colour c = ! isEnabled() ? theme::textDisabled : (getToggleState() ? theme::accent : theme::text);
    if (highlighted || down)
    {
        g.setColour (juce::Colours::black.withAlpha (down ? 0.08f : 0.04f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f);
    }
    if (hasKeyboardFocus (false)) { g.setColour (theme::accent.withAlpha (0.7f)); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 1.0f); }
    const float lw = (float) getProperties().getWithDefault ("stroke", std::max (1.5f, s * 0.068f));
    drawIcon (g, icon, b, c, filled, lw);
}

void IconButton::drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> b, juce::Colour c, bool filled, float lw)
{
    g.setColour (c);
    juce::Path p;
    const float x = b.getX(), y = b.getY(), w = b.getWidth(), h = b.getHeight(), s = std::min (w, h);
    auto stroke = [&] (const juce::Path& path) { g.strokePath (path, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); };
    auto node = [&] (float cx, float cy, float sz, const juce::String& letter)
    {
        juce::Path q; q.addRoundedRectangle (cx - sz * 0.5f, cy - sz * 0.5f, sz, sz, sz * 0.22f);
        g.strokePath (q, juce::PathStrokeType (std::max (1.3f, lw * 0.9f)));
        g.setFont (theme::capFont (sz * 0.5f, 3));
        g.drawText (letter, juce::Rectangle<float> (sz, sz).withCentre ({ cx, cy + 0.3f }), juce::Justification::centred, false);
    };
    switch (icon)
    {
        case Icon::Folder:
            p.startNewSubPath (x + w * 0.08f, y + h * 0.22f); p.lineTo (x + w * 0.38f, y + h * 0.22f); p.lineTo (x + w * 0.47f, y + h * 0.32f);
            p.lineTo (x + w * 0.92f, y + h * 0.32f); p.lineTo (x + w * 0.92f, y + h * 0.80f); p.lineTo (x + w * 0.08f, y + h * 0.80f); p.closeSubPath();
            p.startNewSubPath (x + w * 0.08f, y + h * 0.40f); p.lineTo (x + w * 0.92f, y + h * 0.40f);
            stroke (p); break;
        case Icon::Save:
            p.addRoundedRectangle (x + w * 0.12f, y + h * 0.12f, w * 0.76f, h * 0.76f, s * 0.1f);
            p.addRectangle (x + w * 0.30f, y + h * 0.12f, w * 0.38f, h * 0.22f);
            p.addRoundedRectangle (x + w * 0.27f, y + h * 0.52f, w * 0.46f, h * 0.36f, 1.0f);
            stroke (p); break;
        case Icon::Gear:
        {
            const auto c0 = b.getCentre();
            const int teeth = 8;
            const float ro = s * 0.44f, ri = s * 0.33f;
            for (int i = 0; i < teeth * 4; ++i)
            {
                const float a0 = (float) i * juce::MathConstants<float>::twoPi / (float) (teeth * 4);
                const bool out = (i % 4) == 1 || (i % 4) == 2;
                const auto pt = c0.getPointOnCircumference (out ? ro : ri, a0);
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            p.closeSubPath();
            p.addEllipse (juce::Rectangle<float> (s * 0.30f, s * 0.30f).withCentre (c0));
            stroke (p); break;
        }
        case Icon::Kebab:
            for (int i = 0; i < 3; ++i) g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ b.getCentreX(), b.getCentreY() + 9.5f * (float) (i - 1) }));
            break;
        case Icon::Heart:
        {
            p.startNewSubPath (x + w * 0.5f, y + h * 0.86f);
            p.cubicTo (x - w * 0.12f, y + h * 0.42f, x + w * 0.18f, y - h * 0.02f, x + w * 0.5f, y + h * 0.28f);
            p.cubicTo (x + w * 0.82f, y - h * 0.02f, x + w * 1.12f, y + h * 0.42f, x + w * 0.5f, y + h * 0.86f);
            if (filled) { g.setColour (theme::accent); g.fillPath (p); }
            g.setColour (theme::accent);
            stroke (p); break;
        }
        case Icon::Prev: p.startNewSubPath (x + w * 0.60f, y + h * 0.22f); p.lineTo (x + w * 0.36f, y + h * 0.5f); p.lineTo (x + w * 0.60f, y + h * 0.78f); stroke (p); break;
        case Icon::Next: p.startNewSubPath (x + w * 0.40f, y + h * 0.22f); p.lineTo (x + w * 0.64f, y + h * 0.5f); p.lineTo (x + w * 0.40f, y + h * 0.78f); stroke (p); break;
        case Icon::Panic:
            p.addEllipse (b.reduced (s * 0.08f));
            p.startNewSubPath (x + w * 0.5f, y + h * 0.28f); p.lineTo (x + w * 0.5f, y + h * 0.58f);
            stroke (p);
            g.fillEllipse (juce::Rectangle<float> (lw * 1.7f, lw * 1.7f).withCentre ({ x + w * 0.5f, y + h * 0.71f }));
            break;
        case Icon::TailKill:
            p.startNewSubPath (x + w * 0.04f, y + h * 0.62f);
            p.cubicTo (x + w * 0.14f, y + h * 0.18f, x + w * 0.22f, y + h * 0.18f, x + w * 0.27f, y + h * 0.55f);
            p.cubicTo (x + w * 0.32f, y + h * 0.92f, x + w * 0.40f, y + h * 0.92f, x + w * 0.46f, y + h * 0.45f);
            p.cubicTo (x + w * 0.50f, y + h * 0.20f, x + w * 0.56f, y + h * 0.30f, x + w * 0.60f, y + h * 0.55f);
            p.startNewSubPath (x + w * 0.60f, y + h * 0.30f); p.lineTo (x + w * 0.94f, y + h * 0.66f);
            p.startNewSubPath (x + w * 0.94f, y + h * 0.30f); p.lineTo (x + w * 0.70f, y + h * 0.56f);
            stroke (p); break;
        case Icon::Play: p.addTriangle (x + w * 0.3f, y + h * 0.18f, x + w * 0.3f, y + h * 0.82f, x + w * 0.82f, y + h * 0.5f); g.fillPath (p); break;
        case Icon::Pause: g.fillRect (x + w * 0.28f, y + h * 0.2f, w * 0.15f, h * 0.6f); g.fillRect (x + w * 0.57f, y + h * 0.2f, w * 0.15f, h * 0.6f); break;
        case Icon::Stop: g.fillRoundedRectangle (b.reduced (s * 0.24f), 2.0f); break;
        case Icon::Loop:
            p.addRoundedRectangle (x + w * 0.15f, y + h * 0.3f, w * 0.7f, h * 0.4f, h * 0.2f);
            stroke (p);
            { juce::Path a; a.addTriangle (x + w * 0.62f, y + h * 0.2f, x + w * 0.62f, y + h * 0.4f, x + w * 0.78f, y + h * 0.3f); g.fillPath (a); }
            break;
        case Icon::Record: g.fillEllipse (b.reduced (s * 0.26f)); break;
        case Icon::Export:
            p.startNewSubPath (x + w * 0.5f, y + h * 0.15f); p.lineTo (x + w * 0.5f, y + h * 0.62f);
            p.startNewSubPath (x + w * 0.32f, y + h * 0.45f); p.lineTo (x + w * 0.5f, y + h * 0.63f); p.lineTo (x + w * 0.68f, y + h * 0.45f);
            p.startNewSubPath (x + w * 0.18f, y + h * 0.7f); p.lineTo (x + w * 0.18f, y + h * 0.85f); p.lineTo (x + w * 0.82f, y + h * 0.85f); p.lineTo (x + w * 0.82f, y + h * 0.7f);
            stroke (p); break;
        case Icon::Sliders:
            for (int i = 0; i < 3; ++i)
            {
                const float yy = y + h * (0.22f + 0.28f * (float) i);
                p.startNewSubPath (x + w * 0.06f, yy); p.lineTo (x + w * 0.94f, yy);
                const float kx = x + w * (i == 1 ? 0.36f : (i == 0 ? 0.64f : 0.52f));
                p.addEllipse (kx - s * 0.075f, yy - s * 0.075f, s * 0.15f, s * 0.15f);
            }
            stroke (p); break;
        case Icon::Close:
            p.startNewSubPath (x + w * 0.25f, y + h * 0.25f); p.lineTo (x + w * 0.75f, y + h * 0.75f);
            p.startNewSubPath (x + w * 0.75f, y + h * 0.25f); p.lineTo (x + w * 0.25f, y + h * 0.75f);
            stroke (p); break;
        case Icon::Pin:
        {
            // push pin, tilted: head, collar and needle
            juce::Path pin;
            pin.addRoundedRectangle (-0.16f, -0.50f, 0.32f, 0.34f, 0.06f);
            pin.addTriangle (-0.30f, -0.16f, 0.30f, -0.16f, 0.0f, 0.10f);
            pin.addRectangle (-0.035f, 0.05f, 0.07f, 0.42f);
            pin.applyTransform (juce::AffineTransform::scale (s).rotated (0.62f).translated (b.getCentreX() - s * 0.02f, b.getCentreY() + s * 0.02f));
            if (filled) g.fillPath (pin); else stroke (pin);
            break;
        }
        case Icon::Wave:
            p.startNewSubPath (x + w * 0.04f, y + h * 0.5f);
            for (int i = 1; i <= 40; ++i)
            {
                const float t = (float) i / 40.0f;
                const float amp = 0.36f * (1.0f - 0.55f * t);
                p.lineTo (x + w * (0.04f + 0.92f * t), y + h * (0.5f - amp * std::sin (t * juce::MathConstants<float>::twoPi * 2.0f)));
            }
            stroke (p); break;
        case Icon::Cloud:
            p.startNewSubPath (x + w * 0.22f, y + h * 0.78f);
            p.cubicTo (x + w * 0.02f, y + h * 0.78f, x + w * 0.02f, y + h * 0.50f, x + w * 0.22f, y + h * 0.48f);
            p.cubicTo (x + w * 0.24f, y + h * 0.26f, x + w * 0.46f, y + h * 0.20f, x + w * 0.56f, y + h * 0.34f);
            p.cubicTo (x + w * 0.66f, y + h * 0.22f, x + w * 0.86f, y + h * 0.30f, x + w * 0.80f, y + h * 0.50f);
            p.cubicTo (x + w * 0.98f, y + h * 0.52f, x + w * 0.98f, y + h * 0.78f, x + w * 0.78f, y + h * 0.78f);
            p.closeSubPath();
            stroke (p); break;
        case Icon::RouteParallel:
        {
            const float nx = x + w * 0.78f, sz = h * 0.5f;
            juce::Path d; d.addStar ({ x + w * 0.16f, y + h * 0.5f }, 4, s * 0.07f, s * 0.15f, 0.0f);
            g.fillPath (d);
            p.startNewSubPath (x + w * 0.22f, y + h * 0.5f); p.lineTo (x + w * 0.36f, y + h * 0.5f);
            p.startNewSubPath (x + w * 0.36f, y + h * 0.5f); p.cubicTo (x + w * 0.44f, y + h * 0.5f, x + w * 0.44f, y + h * 0.25f, nx - sz * 0.5f, y + h * 0.25f);
            p.startNewSubPath (x + w * 0.36f, y + h * 0.5f); p.cubicTo (x + w * 0.44f, y + h * 0.5f, x + w * 0.44f, y + h * 0.75f, nx - sz * 0.5f, y + h * 0.75f);
            g.strokePath (p, juce::PathStrokeType (std::max (1.1f, lw * 0.75f)));
            node (nx, y + h * 0.25f, sz, "D");
            node (nx, y + h * 0.75f, sz, "R");
            break;
        }
        case Icon::RouteDR:
        case Icon::RouteRD:
        {
            const float sz = h * 0.82f;
            const bool dr = icon == Icon::RouteDR;
            node (x + w * 0.16f, y + h * 0.5f, sz, dr ? "D" : "R");
            node (x + w * 0.84f, y + h * 0.5f, sz, dr ? "R" : "D");
            p.startNewSubPath (x + w * 0.37f, y + h * 0.5f); p.lineTo (x + w * 0.62f, y + h * 0.5f);
            p.startNewSubPath (x + w * 0.555f, y + h * 0.37f); p.lineTo (x + w * 0.625f, y + h * 0.5f); p.lineTo (x + w * 0.555f, y + h * 0.63f);
            g.strokePath (p, juce::PathStrokeType (std::max (1.2f, lw * 0.85f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
    }
}

// ============================================================================ ChoiceIconButton
ChoiceIconButton::ChoiceIconButton (PluginProcessor& p, int paramIndex, int choiceValue, IconButton::Icon ic, const juce::String& name, const juce::String& tip)
    : juce::Button (name), proc (p), index (paramIndex), choice (choiceValue), icon (ic)
{
    setTooltip (tip);
    setTitle (name);
    setDescription (tip);
    setWantsKeyboardFocus (true);
    setRadioGroupId (1000 + paramIndex);
    attach = std::make_unique<juce::ParameterAttachment> (*proc.param (index), [this] (float v) { setToggleState ((int) std::lround (v) == choice, juce::dontSendNotification); repaint(); });
    attach->sendInitialUpdate();
}

void ChoiceIconButton::clicked()
{
    attach->setValueAsCompleteGesture ((float) choice);
}

void ChoiceIconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    if (on) paintActive (g, r, 6.0f, highlighted); else paintField (g, r, 6.0f, true, highlighted && isEnabled());
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.06f)); g.fillRoundedRectangle (r, 6.0f); }
    if (hasKeyboardFocus (false)) { g.setColour (theme::accent.withAlpha (0.8f)); g.drawRoundedRectangle (r.reduced (1.5f), 6.0f, 1.2f); }
    const juce::Colour c = ! isEnabled() ? theme::textDisabled : (on ? juce::Colours::white : theme::text);
    const auto ic = getProperties().contains ("iconBox") ? juce::Rectangle<float> ((float) getProperties()["iconW"], (float) getProperties()["iconH"]).withCentre (r.getCentre())
                                                         : r.withSizeKeepingCentre (std::min (r.getWidth(), r.getHeight()) * 0.55f, std::min (r.getWidth(), r.getHeight()) * 0.55f);
    IconButton::drawIcon (g, icon, ic, c, on, (float) getProperties().getWithDefault ("stroke", 1.7f));
}

// ============================================================================ EnableDot
EnableDot::EnableDot (PluginProcessor& p, int paramIndex, const juce::String& what) : proc (p), index (paramIndex)
{
    setTitle ("Enable " + what);
    setTooltip ("Turn the " + what + " on or off (changes processing, not just the display)");
    setWantsKeyboardFocus (true);
}

void EnableDot::paint (juce::Graphics& g)
{
    const bool on = proc.value (index) > 0.5f;
    auto r = getLocalBounds().toFloat().withSizeKeepingCentre (13.0f, 13.0f);
    if (on)
    {
        g.setColour (theme::accent.withAlpha (0.14f));
        g.fillEllipse (r.expanded (3.0f));
        g.setColour (theme::accent.withAlpha (0.20f));
        g.fillEllipse (r.expanded (1.4f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff7a3c), r.getX(), r.getY(), juce::Colour (0xffe8461a), r.getX(), r.getBottom(), false));
        g.fillEllipse (r);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (r.reduced (3.0f).translated (-1.0f, -1.5f).withHeight (2.5f));
    }
    else
    {
        g.setColour (juce::Colour (0xffd9d1c3));
        g.fillEllipse (r);
        g.setColour (theme::borderDark);
        g.drawEllipse (r, 1.0f);
    }
    if (hasKeyboardFocus (false)) { g.setColour (theme::accent); g.drawEllipse (r.expanded (4.0f), 1.0f); }
}

void EnableDot::toggle()
{
    const bool turnOn = proc.value (index) < 0.5f;
    // legacy sessions may carry Harmony Method = Off: switching harmony on always runs Classic
    if (turnOn && index == HarmEnable && proc.value (HarmMethod) < 0.5f) proc.setValue (HarmMethod, 1.0f);
    proc.setValue (index, turnOn ? 1.0f : 0.0f);
    repaint();
}
void EnableDot::mouseDown (const juce::MouseEvent&) { toggle(); }
bool EnableDot::keyPressed (const juce::KeyPress& k)
{
    if (k.isKeyCode (juce::KeyPress::spaceKey) || k.isKeyCode (juce::KeyPress::returnKey)) { toggle(); return true; }
    return false;
}

// ============================================================================ ParamStepper
ParamStepper::ParamStepper (PluginProcessor& p, int paramIndex) : proc (p), index (paramIndex)
{
    for (auto* b : { &minus, &plus }) { addAndMakeVisible (*b); }
    minus.onClick = [this] { step (-1); };
    plus.onClick = [this] { step (1); };
    attach = std::make_unique<juce::ParameterAttachment> (*proc.param (index), [this] (float) { repaint(); });
    attach->sendInitialUpdate();
    setTitle (paramInfo (index).name);
    setTooltip (paramInfo (index).help);
}
void ParamStepper::step (int d)
{
    const auto& info = paramInfo (index);
    attach->setValueAsCompleteGesture (juce::jlimit (info.minV, info.maxV, proc.value (index) + (float) d));
}
void ParamStepper::resized()
{
    auto r = getLocalBounds();
    minus.setBounds (r.removeFromLeft (r.getHeight()));
    plus.setBounds (r.removeFromRight (r.getHeight()));
}
void ParamStepper::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (getHeight(), 0).toFloat();
    g.setColour (theme::valueBox);
    g.fillRect (r);
    g.setColour (theme::border);
    g.drawRect (r, 1.0f);
    g.setColour (isEnabled() ? theme::text : theme::textDisabled);
    g.setFont (theme::font (12.5f, 1));
    g.drawFittedText ((prefix.isNotEmpty() ? prefix + " " : juce::String()) + juce::String (paramValueToText (index, proc.value (index))), r.toNearestInt(), juce::Justification::centred, 1, 0.8f);
}

// ============================================================================ LevelMeter
void LevelMeter::setLevels (float l, float r)
{
    const float in[2] = { l, r };
    for (int i = 0; i < 2; ++i)
    {
        const float db = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (in[i], -60.0f) + 60.0f) / 60.0f);
        lv[i] = std::max (db, lv[i] * 0.86f);
        hold[i] = std::max (db, hold[i] - 0.01f);
    }
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::text);
    g.setFont (theme::capFont (8.5f, 1));
    g.drawText (name, r.removeFromTop (14.0f), juce::Justification::centred);
    r.removeFromTop (3.0f);
    const float bw = 7.0f, gap = 4.5f;
    const float x0 = r.getCentreX() - bw - gap * 0.5f;
    for (int i = 0; i < 2; ++i)
    {
        auto bar = juce::Rectangle<float> (x0 + (float) i * (bw + gap), r.getY(), bw, r.getHeight());
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3f3c), bar.getX(), bar.getY(), juce::Colour (0xff222826), bar.getRight(), bar.getY(), false));
        g.fillRoundedRectangle (bar, 3.5f);
        g.setColour (juce::Colour (0x40000000));
        g.drawRoundedRectangle (bar, 3.5f, 0.8f);
        const float lvl = lv[i];
        if (lvl > 0.003f)
        {
            auto fill = bar.reduced (1.2f);
            fill = fill.withTop (fill.getBottom() - fill.getHeight() * lvl);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff7a3a), fill.getX(), bar.getY(), juce::Colour (0xffe8541c), fill.getX(), bar.getBottom(), false));
            g.fillRoundedRectangle (fill, 2.5f);
        }
        if (hold[i] > 0.01f)
        {
            const float hy = bar.getBottom() - 1.2f - (bar.getHeight() - 2.4f) * hold[i];
            g.setColour (juce::Colour (0xffffd2b0).withAlpha (0.8f));
            g.fillRect (bar.getX() + 1.5f, hy - 0.5f, bw - 3.0f, 1.0f);
        }
    }
}
} // namespace pa
