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
ParamKnob::ParamKnob (PluginProcessor& p, int paramIndex, const juce::String& title, bool compactStyle) : proc (p), compact (compactStyle)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setMouseDragSensitivity (220);
    // Shift-drag swaps to fine velocity mode
    slider.setVelocityModeParameters (0.35, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    slider.setWantsKeyboardFocus (true);
    addAndMakeVisible (slider);
    value.setJustificationType (juce::Justification::centred);
    value.setEditable (false, true, false);
    value.setFont (theme::font (14.0f));
    value.setColour (juce::Label::textColourId, theme::text);
    value.setColour (juce::Label::textWhenEditingColourId, theme::text);
    value.setColour (juce::Label::backgroundWhenEditingColourId, theme::valueBox);
    value.setColour (juce::Label::outlineWhenEditingColourId, theme::accent);
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
    slider.setTitle (paramInfo (index).name);
    slider.setDescription (paramInfo (index).help);
    slider.setTooltip (juce::String (paramInfo (index).help) + "  (double-click: default, shift-drag: fine)");
    value.setTooltip ("Double-click to type a value");
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
    r.removeFromTop (18);
    auto box = r.removeFromBottom (26);
    value.setBounds (box.withSizeKeepingCentre (std::min (box.getWidth(), 80), 24));
    slider.setBounds (r.reduced (2));
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
    g.setColour (isEnabled() ? theme::text : theme::textDisabled);
    g.setFont (theme::font (12.5f, 1));
    g.drawText (titleText.toUpperCase(), getLocalBounds().removeFromTop (18), juce::Justification::centred);
    auto box = value.getBounds().toFloat();
    g.setColour (theme::valueBox);
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.05f));
    g.drawHorizontalLine ((int) box.getY() + 1, box.getX() + 3, box.getRight() - 3);
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
    const float rad = 5.0f;
    g.setGradientFill (juce::ColourGradient (theme::cardTop.brighter (0.03f), r.getX(), r.getY(), theme::cardBottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    const int n = items.size();
    const float w = r.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto cell = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
        if (i == sel)
        {
            juce::Path p;
            p.addRoundedRectangle (cell.getX(), cell.getY(), cell.getWidth(), cell.getHeight(), rad, rad, i == 0, i == n - 1, i == 0, i == n - 1);
            g.setGradientFill (juce::ColourGradient (isEnabled() ? theme::accentTop : theme::border, cell.getX(), cell.getY(),
                                                     isEnabled() ? theme::accentBottom : theme::borderDark, cell.getX(), cell.getBottom(), false));
            g.fillPath (p);
        }
        else if (i > 0 && i != sel + 1)
        {
            g.setColour (theme::border);
            g.drawVerticalLine ((int) cell.getX(), cell.getY() + 4.0f, cell.getBottom() - 4.0f);
        }
        g.setColour (i == sel ? juce::Colours::white : (isEnabled() ? theme::text : theme::textDisabled));
        g.setFont (theme::font (fontH, 1));
        g.drawFittedText (items[i], cell.reduced (3.0f, 1.0f).toNearestInt(), juce::Justification::centred, 1, 0.75f);
    }
    g.setColour (hasKeyboardFocus (false) ? theme::accent : theme::border);
    g.drawRoundedRectangle (r, rad, 1.0f);
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled()) return;
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
    const float s = std::min (r.getWidth(), r.getHeight());
    auto b = r.withSizeKeepingCentre (s, s);
    const juce::Colour c = ! isEnabled() ? theme::textDisabled : (getToggleState() ? theme::accent : theme::text);
    if (highlighted || down)
    {
        g.setColour (juce::Colours::black.withAlpha (down ? 0.08f : 0.04f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f);
    }
    if (hasKeyboardFocus (false)) { g.setColour (theme::accent.withAlpha (0.7f)); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 1.0f); }
    g.setColour (c);
    juce::Path p;
    const float x = b.getX(), y = b.getY(), w = b.getWidth(), h = b.getHeight();
    const float lw = std::max (1.4f, s * 0.07f);
    auto stroke = [&] (const juce::Path& path) { g.strokePath (path, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); };
    switch (icon)
    {
        case Icon::Folder:
            p.startNewSubPath (x + w * 0.1f, y + h * 0.25f); p.lineTo (x + w * 0.4f, y + h * 0.25f); p.lineTo (x + w * 0.5f, y + h * 0.35f);
            p.lineTo (x + w * 0.9f, y + h * 0.35f); p.lineTo (x + w * 0.9f, y + h * 0.82f); p.lineTo (x + w * 0.1f, y + h * 0.82f); p.closeSubPath();
            stroke (p); break;
        case Icon::Save:
            p.addRoundedRectangle (x + w * 0.15f, y + h * 0.15f, w * 0.7f, h * 0.7f, 2.0f);
            p.addRectangle (x + w * 0.3f, y + h * 0.15f, w * 0.4f, h * 0.22f);
            p.addRectangle (x + w * 0.3f, y + h * 0.55f, w * 0.4f, h * 0.3f);
            stroke (p); break;
        case Icon::Gear:
        {
            const auto c0 = b.getCentre();
            for (int i = 0; i < 8; ++i)
            {
                const float a = (float) i * juce::MathConstants<float>::twoPi / 8.0f;
                p.startNewSubPath (c0.getPointOnCircumference (s * 0.28f, a));
                p.lineTo (c0.getPointOnCircumference (s * 0.42f, a));
            }
            p.addEllipse (juce::Rectangle<float> (s * 0.56f, s * 0.56f).withCentre (c0));
            p.addEllipse (juce::Rectangle<float> (s * 0.2f, s * 0.2f).withCentre (c0));
            stroke (p); break;
        }
        case Icon::Kebab:
            for (int i = 0; i < 3; ++i) g.fillEllipse (juce::Rectangle<float> (lw * 2.2f, lw * 2.2f).withCentre ({ b.getCentreX(), y + h * (0.22f + 0.28f * (float) i) }));
            break;
        case Icon::Heart:
        {
            p.startNewSubPath (x + w * 0.5f, y + h * 0.85f);
            p.cubicTo (x - w * 0.15f, y + h * 0.4f, x + w * 0.2f, y - h * 0.05f, x + w * 0.5f, y + h * 0.3f);
            p.cubicTo (x + w * 0.8f, y - h * 0.05f, x + w * 1.15f, y + h * 0.4f, x + w * 0.5f, y + h * 0.85f);
            if (filled) { g.setColour (theme::accent); g.fillPath (p); }
            g.setColour (filled ? theme::accent : theme::accent.withAlpha (0.9f));
            stroke (p); break;
        }
        case Icon::Prev: p.startNewSubPath (x + w * 0.62f, y + h * 0.2f); p.lineTo (x + w * 0.35f, y + h * 0.5f); p.lineTo (x + w * 0.62f, y + h * 0.8f); stroke (p); break;
        case Icon::Next: p.startNewSubPath (x + w * 0.38f, y + h * 0.2f); p.lineTo (x + w * 0.65f, y + h * 0.5f); p.lineTo (x + w * 0.38f, y + h * 0.8f); stroke (p); break;
        case Icon::Panic:
            p.addEllipse (b.reduced (s * 0.12f));
            p.startNewSubPath (x + w * 0.5f, y + h * 0.3f); p.lineTo (x + w * 0.5f, y + h * 0.58f);
            stroke (p);
            g.fillEllipse (juce::Rectangle<float> (lw * 1.6f, lw * 1.6f).withCentre ({ x + w * 0.5f, y + h * 0.7f }));
            break;
        case Icon::TailKill:
            p.startNewSubPath (x + w * 0.1f, y + h * 0.5f);
            for (int i = 1; i <= 16; ++i) { const float t = (float) i / 16.0f; p.lineTo (x + w * (0.1f + 0.8f * t), y + h * (0.5f - 0.3f * (1.0f - t) * std::sin (t * 14.0f))); }
            p.startNewSubPath (x + w * 0.62f, y + h * 0.2f); p.lineTo (x + w * 0.9f, y + h * 0.48f);
            p.startNewSubPath (x + w * 0.9f, y + h * 0.2f); p.lineTo (x + w * 0.62f, y + h * 0.48f);
            stroke (p); break;
        case Icon::Play: p.addTriangle (x + w * 0.3f, y + h * 0.18f, x + w * 0.3f, y + h * 0.82f, x + w * 0.82f, y + h * 0.5f); g.fillPath (p); break;
        case Icon::Pause: g.fillRect (x + w * 0.28f, y + h * 0.2f, w * 0.15f, h * 0.6f); g.fillRect (x + w * 0.57f, y + h * 0.2f, w * 0.15f, h * 0.6f); break;
        case Icon::Stop: g.fillRoundedRectangle (b.reduced (s * 0.24f), 2.0f); break;
        case Icon::Loop:
            p.addRoundedRectangle (x + w * 0.15f, y + h * 0.3f, w * 0.7f, h * 0.4f, h * 0.2f);
            stroke (p);
            { juce::Path a; a.addTriangle (x + w * 0.62f, y + h * 0.2f, x + w * 0.62f, y + h * 0.4f, x + w * 0.78f, y + h * 0.3f); g.fillPath (a); }
            break;
        case Icon::Record: g.setColour (getToggleState() ? theme::accent : c); g.fillEllipse (b.reduced (s * 0.26f)); break;
        case Icon::Export:
            p.startNewSubPath (x + w * 0.5f, y + h * 0.15f); p.lineTo (x + w * 0.5f, y + h * 0.62f);
            p.startNewSubPath (x + w * 0.32f, y + h * 0.45f); p.lineTo (x + w * 0.5f, y + h * 0.63f); p.lineTo (x + w * 0.68f, y + h * 0.45f);
            p.startNewSubPath (x + w * 0.18f, y + h * 0.7f); p.lineTo (x + w * 0.18f, y + h * 0.85f); p.lineTo (x + w * 0.82f, y + h * 0.85f); p.lineTo (x + w * 0.82f, y + h * 0.7f);
            stroke (p); break;
        case Icon::Sliders:
            for (int i = 0; i < 3; ++i)
            {
                const float yy = y + h * (0.25f + 0.25f * (float) i);
                p.startNewSubPath (x + w * 0.12f, yy); p.lineTo (x + w * 0.88f, yy);
                const float kx = x + w * (i == 1 ? 0.32f : (i == 0 ? 0.65f : 0.5f));
                p.addEllipse (kx - s * 0.07f, yy - s * 0.07f, s * 0.14f, s * 0.14f);
            }
            stroke (p); break;
        case Icon::Close:
            p.startNewSubPath (x + w * 0.25f, y + h * 0.25f); p.lineTo (x + w * 0.75f, y + h * 0.75f);
            p.startNewSubPath (x + w * 0.75f, y + h * 0.25f); p.lineTo (x + w * 0.25f, y + h * 0.75f);
            stroke (p); break;
    }
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
        g.setColour (theme::accent.withAlpha (0.25f));
        g.fillEllipse (r.expanded (2.5f));
        g.setGradientFill (juce::ColourGradient (theme::accentTop, r.getX(), r.getY(), theme::accentBottom, r.getX(), r.getBottom(), false));
        g.fillEllipse (r);
    }
    else
    {
        g.setColour (theme::border);
        g.fillEllipse (r);
        g.setColour (theme::borderDark);
        g.drawEllipse (r, 1.0f);
    }
    if (hasKeyboardFocus (false)) { g.setColour (theme::accent); g.drawEllipse (r.expanded (4.0f), 1.0f); }
}

void EnableDot::toggle() { proc.setValue (index, proc.value (index) > 0.5f ? 0.0f : 1.0f); repaint(); }
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
    g.setFont (theme::font (10.5f, 1));
    g.drawText (name, r.removeFromTop (13.0f), juce::Justification::centred);
    const float bw = 7.0f, gap = 4.0f;
    const float x0 = r.getCentreX() - bw - gap * 0.5f;
    for (int i = 0; i < 2; ++i)
    {
        auto bar = juce::Rectangle<float> (x0 + (float) i * (bw + gap), r.getY() + 2.0f, bw, r.getHeight() - 4.0f);
        g.setColour (theme::display);
        g.fillRoundedRectangle (bar, 3.0f);
        auto fill = bar.reduced (1.5f);
        fill = fill.withTop (fill.getBottom() - fill.getHeight() * lv[i]);
        g.setGradientFill (juce::ColourGradient (theme::accentTop, fill.getX(), bar.getY(), theme::accentBottom, fill.getX(), bar.getBottom(), false));
        g.fillRoundedRectangle (fill, 2.0f);
        const float hy = bar.getBottom() - 1.5f - (bar.getHeight() - 3.0f) * hold[i];
        g.setColour (theme::ivoryTrace.withAlpha (0.8f));
        g.drawHorizontalLine ((int) hy, bar.getX() + 1.5f, bar.getRight() - 1.5f);
    }
}
} // namespace pa
