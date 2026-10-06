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
    return { x, 0.0f, whiteW * 0.64f, h * 0.585f };
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
    const float H = (float) getHeight();
    // keybed backing (visible as the fine separations between keys)
    g.setColour (juce::Colour (0xffbdb4a5));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
    int count = 0;
    for (int n = low; count < whites && n < 128; ++n)
    {
        if (isBlack (n)) continue;
        auto r = keyRect (n).withTrimmedLeft (count == 0 ? 0.6f : 0.5f).withTrimmedRight (0.5f).withTrimmedTop (0.6f).withTrimmedBottom (0.6f);
        const float on = voiced[(size_t) n], rel = releasing[(size_t) n];
        juce::Path key;
        key.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 4.0f, 4.0f, count == 0, count == whites - 1, true, true);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffdfbf7), r.getX(), r.getY(), juce::Colour (0xffefebe3), r.getX(), r.getBottom(), false));
        g.fillPath (key);
        if (on > 0.0f || rel > 0.01f)
        {
            const float a = on > 0.0f ? 1.0f : juce::jlimit (0.15f, 0.55f, rel);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffa06e).withAlpha (a), r.getX(), r.getY(), juce::Colour (0xffff874a).withAlpha (a), r.getX(), r.getBottom(), false));
            g.fillPath (key);
        }
        // small lower shadow and a faint light top edge
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.fillRect (r.getX() + 1.0f, r.getBottom() - 2.0f, r.getWidth() - 2.0f, 1.2f);
        if (n == mouseNote || qwertyDown[(size_t) n]) { g.setColour (juce::Colours::black.withAlpha (0.10f)); g.fillPath (key); }
        if (customMark[(size_t) n]) { g.setColour (theme::accent); g.strokePath (key, juce::PathStrokeType (2.0f)); }
        if (on > 0.0f || n % 12 == 0)
        {
            const bool active = on > 0.0f;
            g.setColour (active ? juce::Colour (0xff2a1a12) : juce::Colour (0xffa6a093));
            g.setFont (theme::capFont (active ? 10.5f : 10.0f, active ? 2 : 0));
            g.drawText (active ? noteName (n, sharps, false) : noteName (n, sharps, true), r.withTop (r.getBottom() - 30.0f).withTrimmedBottom (8.0f), juce::Justification::centred);
        }
        ++count;
    }
    count = 0;
    for (int n = low; count < whites && n < 128; ++n)
    {
        if (! isBlack (n)) { ++count; continue; }
        auto r = keyRect (n).withTrimmedTop (0.6f);
        const float on = voiced[(size_t) n], rel = releasing[(size_t) n];
        juce::Path key;
        key.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 3.0f, 3.0f, false, false, true, true);
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillRoundedRectangle (r.translated (1.2f, 1.6f), 3.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3a37), r.getX(), r.getY(), juce::Colour (0xff121212), r.getX(), r.getBottom(), false));
        g.fillPath (key);
        if (on > 0.0f || rel > 0.01f)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff6a2c), r.getX(), r.getY(), juce::Colour (0xffe2460f), r.getX(), r.getBottom(), false));
            g.setOpacity (on > 0.0f ? 1.0f : juce::jlimit (0.2f, 0.6f, rel));
            g.fillRoundedRectangle (r.reduced (1.4f), 2.5f);
            g.setOpacity (1.0f);
        }
        // brighter top edge, darker lower edge (dimensional)
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.fillRect (r.getX() + 2.0f, r.getY() + 1.5f, r.getWidth() - 4.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (r.getX() + 2.0f, r.getBottom() - 4.0f, r.getWidth() - 4.0f, 2.0f);
        if (n == mouseNote || qwertyDown[(size_t) n]) { g.setColour (juce::Colours::white.withAlpha (0.15f)); g.fillPath (key); }
        if (customMark[(size_t) n]) { g.setColour (theme::accent); g.strokePath (key, juce::PathStrokeType (2.0f)); }
    }
    juce::ignoreUnused (H);
    if (hasKeyboardFocus (false)) { g.setColour (theme::accent.withAlpha (0.6f)); g.drawRoundedRectangle (getLocalBounds().toFloat(), 4.0f, 1.0f); }
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
    setTooltip (pitch ? "Pitch bend for harmony voices (range: Settings > MIDI / Setup). Springs back." : "Mod wheel: adds to Harmony Depth or Motion (target: Settings > MIDI / Setup).");
}

void Wheel::paint (juce::Graphics& g)
{
    // canonical geometry: 26 x 78 dark well at the top centre, label cap top 86 px below the well top
    auto r = getLocalBounds().toFloat();
    auto slot = juce::Rectangle<float> (26.0f, 78.0f).withCentre ({ r.getCentreX(), 39.0f });
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawRoundedRectangle (slot.expanded (0.8f).translated (0.0f, 0.8f), 9.0f, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c3331), slot.getX(), slot.getY(), juce::Colour (0xff1b2120), slot.getRight(), slot.getY(), false));
    g.fillRoundedRectangle (slot, 8.5f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (slot, 8.5f, 1.0f);
    const float norm = isPitch ? (value * 0.5f + 0.5f) : value;
    const float y = slot.getBottom() - 15.0f - norm * (slot.getHeight() - 30.0f);
    auto thumb = juce::Rectangle<float> (slot.getWidth() - 8.0f, 17.0f).withCentre ({ slot.getCentreX(), y });
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (thumb.translated (0.0f, 1.5f), 3.5f);
    if (isPitch)
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff9a68), thumb.getX(), thumb.getY(), juce::Colour (0xffef6a34), thumb.getX(), thumb.getBottom(), false));
    else
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff8f2e8), thumb.getX(), thumb.getY(), juce::Colour (0xffe1d8ca), thumb.getX(), thumb.getBottom(), false));
    g.fillRoundedRectangle (thumb, 3.5f);
    g.setColour (isPitch ? juce::Colour (0xffd84a14) : theme::accent);
    g.fillRect (thumb.withSizeKeepingCentre (thumb.getWidth() * 0.55f, 2.0f));
    g.setColour (theme::textLabel);
    g.setFont (theme::capFont (9.0f, 1));
    g.drawText (isPitch ? "PITCH" : "MOD", juce::Rectangle<float> (0.0f, 84.0f, r.getWidth(), 14.0f), juce::Justification::centred, false);
}

void Wheel::mouseDown (const juce::MouseEvent& e) { mouseDrag (e); }

void Wheel::mouseDrag (const juce::MouseEvent& e)
{
    // matches the 78 px well drawn in paint(): handle travel from y 15 to y 63
    const float norm = juce::jlimit (0.0f, 1.0f, (63.0f - (float) e.y) / 48.0f);
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
