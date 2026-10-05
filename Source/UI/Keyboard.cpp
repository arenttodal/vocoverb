#include "Keyboard.h"

namespace pa
{
HarmonyKeyboard::HarmonyKeyboard (PluginProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    setTitle ("Harmony keyboard");
    setDescription ("Plays harmony notes (MIDI/Arp source), sets the root (Chord/Intervals) or toggles custom chord notes. Silent without input audio or a tail.");
    setTooltip ("Harmony keys: MIDI/Arp = play notes; Chord = set root (Custom: toggle notes); Intervals = set root. Computer keys A-K also play.");
}

void HarmonyKeyboard::setRange (int lowNote, int numWhiteKeys) { low = lowNote; whites = numWhiteKeys; repaint(); }

juce::Rectangle<float> HarmonyKeyboard::keyRect (int note) const
{
    // white key index
    auto whiteIndex = [this] (int n) { int c = 0; for (int k = low; k < n; ++k) if (! isBlack (k)) ++c; return c; };
    const float h = (float) getHeight();
    if (! isBlack (note)) return { (float) whiteIndex (note) * whiteW, 0.0f, whiteW, h };
    const float x = (float) whiteIndex (note) * whiteW - whiteW * 0.32f;
    return { x, 0.0f, whiteW * 0.64f, h * 0.6f };
}

int HarmonyKeyboard::noteAt (juce::Point<float> p) const
{
    int lastWhite = low;
    int count = 0;
    for (int n = low; count <= whites && n < 128; ++n)
    {
        if (isBlack (n) && keyRect (n).contains (p)) return n;
        if (! isBlack (n)) ++count;
        lastWhite = n;
    }
    count = 0;
    for (int n = low; count < whites && n < 128; ++n)
    {
        if (! isBlack (n)) { if (keyRect (n).contains (p)) return n; ++count; }
    }
    juce::ignoreUnused (lastWhite);
    return -1;
}

void HarmonyKeyboard::update()
{
    auto& t = proc.telemetry();
    voiced.fill (0.0f); releasing.fill (0.0f); customMark.fill (false);
    for (int v = 0; v < 6; ++v)
    {
        const int n = t.voiceNote[(size_t) v].load();
        if (n < 0 || n > 127) continue;
        const float e = t.voiceEnv[(size_t) v].load();
        if (t.voiceGate[(size_t) v].load()) voiced[(size_t) n] = std::max (voiced[(size_t) n], std::max (0.35f, e));
        else releasing[(size_t) n] = std::max (releasing[(size_t) n], e);
    }
    if ((int) proc.value (NoteSource) == 1 && (int) proc.value (ChQuality) == 8)
        for (int k = 0; k < 6; ++k) { const int n = (int) proc.value (ChCustom1 + k); if (n >= 0 && n < 128) customMark[(size_t) n] = true; }
    repaint();
}

void HarmonyKeyboard::paint (juce::Graphics& g)
{
    whiteW = (float) getWidth() / (float) whites;
    const bool sharps = keyPrefersSharps ((int) proc.value (ChRoot));
    int count = 0;
    for (int n = low; count < whites && n < 128; ++n)
    {
        if (isBlack (n)) continue;
        auto r = keyRect (n).reduced (0.6f, 0.0f);
        const float on = voiced[(size_t) n], rel = releasing[(size_t) n];
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffbfaf6), r.getX(), r.getY(), juce::Colour (0xffe9e4da), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        if (on > 0.0f || rel > 0.01f)
        {
            const auto c = on > 0.0f ? theme::accent : theme::amber;
            const float a = on > 0.0f ? 0.9f : juce::jlimit (0.15f, 0.5f, rel);
            g.setGradientFill (juce::ColourGradient (c.withAlpha (a * 0.35f), r.getX(), r.getY(), c.withAlpha (a), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 3.0f);
        }
        if (n == mouseNote || qwertyDown[(size_t) n]) { g.setColour (juce::Colours::black.withAlpha (0.12f)); g.fillRoundedRectangle (r, 3.0f); }
        if (customMark[(size_t) n]) { g.setColour (theme::accent); g.drawRoundedRectangle (r.reduced (2.0f), 3.0f, 2.0f); }
        g.setColour (juce::Colour (0xff6d675c));
        g.drawRoundedRectangle (r, 3.0f, 0.8f);
        if (on > 0.0f || n % 12 == 0)
        {
            g.setColour (on > 0.0f ? theme::text : theme::textMuted);
            g.setFont (theme::font (11.5f, on > 0.0f ? 2 : 0));
            g.drawText (on > 0.0f ? noteName (n, sharps, false) : noteName (n, sharps, true), r.withTop (r.getBottom() - 18.0f), juce::Justification::centred);
        }
        ++count;
    }
    count = 0;
    for (int n = low; count < whites && n < 128; ++n)
    {
        if (! isBlack (n)) { ++count; continue; }
        auto r = keyRect (n);
        const float on = voiced[(size_t) n], rel = releasing[(size_t) n];
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 3.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3833), r.getX(), r.getY(), juce::Colour (0xff141412), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        if (on > 0.0f || rel > 0.01f)
        {
            const auto c = on > 0.0f ? theme::accent : theme::amber;
            g.setGradientFill (juce::ColourGradient (c.brighter (0.2f), r.getX(), r.getY(), c.darker (0.2f), r.getX(), r.getBottom(), false));
            g.setOpacity (on > 0.0f ? 1.0f : juce::jlimit (0.2f, 0.6f, rel));
            g.fillRoundedRectangle (r.reduced (1.5f), 2.5f);
            g.setOpacity (1.0f);
        }
        if (n == mouseNote || qwertyDown[(size_t) n]) { g.setColour (juce::Colours::white.withAlpha (0.15f)); g.fillRoundedRectangle (r, 3.0f); }
        if (customMark[(size_t) n]) { g.setColour (theme::accent); g.drawRoundedRectangle (r.reduced (1.0f), 3.0f, 2.0f); }
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawHorizontalLine ((int) r.getY() + 2, r.getX() + 3.0f, r.getRight() - 3.0f);
    }
    if (hasKeyboardFocus (false)) { g.setColour (theme::accent.withAlpha (0.6f)); g.drawRect (getLocalBounds(), 1); }
}

void HarmonyKeyboard::press (int note)
{
    if (note < 0) return;
    const int src = (int) proc.value (NoteSource);
    if (src == 1) // Chord: set root, or toggle custom notes
    {
        if ((int) proc.value (ChQuality) == 8)
        {
            int freeSlot = -1;
            for (int k = 0; k < 6; ++k)
            {
                const int v = (int) proc.value (ChCustom1 + k);
                if (v == note) { proc.setValue (ChCustom1 + k, -1.0f); return; }
                if (v < 0 && freeSlot < 0) freeSlot = k;
            }
            if (freeSlot >= 0) proc.setValue (ChCustom1 + freeSlot, (float) note);
        }
        else
        {
            proc.setValue (ChRoot, (float) (note % 12));
            proc.setValue (ChOctave, (float) juce::jlimit (1, 6, note / 12 - 1));
        }
        return;
    }
    if (src == 2 && (int) proc.value (IntRefSource) == 0) { proc.setValue (IntRoot, (float) juce::jlimit (24, 96, note)); return; }
    mouseNote = note;
    proc.sendKeyboardMidi (0x90, (uint8_t) note, 100);
}

void HarmonyKeyboard::release()
{
    if (mouseNote >= 0) proc.sendKeyboardMidi (0x80, (uint8_t) mouseNote, 0);
    mouseNote = -1;
}

void HarmonyKeyboard::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    press (noteAt (e.position));
    repaint();
}

void HarmonyKeyboard::mouseDrag (const juce::MouseEvent& e)
{
    const int src = (int) proc.value (NoteSource);
    if (src == 1 || (src == 2 && (int) proc.value (IntRefSource) == 0)) return;
    const int n = noteAt (e.position);
    if (n != mouseNote && n >= 0) { release(); press (n); repaint(); }
}

void HarmonyKeyboard::mouseUp (const juce::MouseEvent&) { release(); repaint(); }

bool HarmonyKeyboard::keyPressed (const juce::KeyPress&) { return false; }

bool HarmonyKeyboard::keyStateChanged (bool)
{
    static const char* keys = "awsedftgyhujk";
    const int base = 48;
    bool used = false;
    for (int i = 0; keys[i] != 0; ++i)
    {
        const int note = base + i;
        const bool down = juce::KeyPress::isKeyCurrentlyDown (keys[i]);
        if (down != qwertyDown[(size_t) note])
        {
            qwertyDown[(size_t) note] = down;
            const int src = (int) proc.value (NoteSource);
            if (src == 0 || src == 3) proc.sendKeyboardMidi (down ? 0x90 : 0x80, (uint8_t) note, down ? 100 : 0);
            else if (down) press (note);
            used = true;
        }
    }
    if (used) repaint();
    return used;
}

// ============================================================================ wheels
Wheel::Wheel (PluginProcessor& p, bool pitch) : proc (p), isPitch (pitch), value (pitch ? 0.0f : 0.0f)
{
    setTitle (pitch ? "Pitch wheel" : "Mod wheel");
    setTooltip (pitch ? "Pitch bend for harmony voices (range in Advanced > MIDI). Springs back." : "Mod wheel: adds to Harmony Depth or Motion (Advanced > MIDI).");
}

void Wheel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto slot = r.withTrimmedBottom (18.0f).reduced (r.getWidth() * 0.22f, 2.0f);
    g.setColour (theme::display);
    g.fillRoundedRectangle (slot, slot.getWidth() * 0.45f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (slot, slot.getWidth() * 0.45f, 1.0f);
    const float norm = isPitch ? (value * 0.5f + 0.5f) : value;
    const float y = slot.getBottom() - 10.0f - norm * (slot.getHeight() - 20.0f);
    auto thumb = juce::Rectangle<float> (slot.getWidth() - 6.0f, 16.0f).withCentre ({ slot.getCentreX(), y });
    g.setGradientFill (juce::ColourGradient (theme::knobFaceTop, thumb.getX(), thumb.getY(), theme::knobFaceBottom, thumb.getX(), thumb.getBottom(), false));
    g.fillRoundedRectangle (thumb, 4.0f);
    g.setColour (theme::accent);
    g.fillRect (thumb.withSizeKeepingCentre (thumb.getWidth() * 0.4f, 2.5f));
    g.setColour (theme::text);
    g.setFont (theme::font (11.0f, 1));
    g.drawText (isPitch ? "PITCH" : "MOD", r.removeFromBottom (16.0f), juce::Justification::centred);
}

void Wheel::mouseDown (const juce::MouseEvent& e) { mouseDrag (e); }

void Wheel::mouseDrag (const juce::MouseEvent& e)
{
    const float h = (float) getHeight() - 38.0f;
    const float norm = juce::jlimit (0.0f, 1.0f, 1.0f - ((float) e.y - 12.0f) / h);
    value = isPitch ? norm * 2.0f - 1.0f : norm;
    send();
    repaint();
}

void Wheel::mouseUp (const juce::MouseEvent&)
{
    if (isPitch) { value = 0.0f; send(); repaint(); }
}

void Wheel::send()
{
    if (isPitch)
    {
        const int v = juce::jlimit (0, 16383, (int) std::round ((value * 0.5f + 0.5f) * 16383.0f));
        proc.sendKeyboardMidi (0xE0, (uint8_t) (v & 0x7f), (uint8_t) (v >> 7));
    }
    else proc.sendKeyboardMidi (0xB0, 1, (uint8_t) juce::jlimit (0, 127, (int) std::round (value * 127.0f)));
}
} // namespace pa
