#include "SettingsViews.h"

#include "Presets/PresetManager.h"
#include "Standalone/StandaloneHost.h"

namespace pa
{
juce::String midiStatusFor (PluginProcessor& proc);

namespace
{
constexpr float kLabelCap = 8.6f, kValueCap = 8.6f;

juce::String firstSentence (const juce::String& s)
{
    const int dot = s.indexOf (". ");
    return dot > 0 ? s.substring (0, dot + 1) : s;
}

/** Snapshot recall/store buttons for the stored chord. */
struct SnapshotStrip : public juce::Component
{
    explicit SnapshotStrip (PluginProcessor& p) : proc (p)
    {
        for (int i = 0; i < 8; ++i)
        {
            auto* b = buttons.add (new juce::TextButton (juce::String (i + 1)));
            b->setTooltip ("Chord snapshot " + juce::String (i + 1) + ": click to recall (arm STORE first to save). MIDI Program Change " + juce::String (i) + " recalls it too.");
            b->getProperties().set ("capPx", 8.0f);
            b->onClick = [this, i] {
                if (store.getToggleState()) { proc.storeSnapshot (i); store.setToggleState (false, juce::dontSendNotification); }
                else proc.recallSnapshot (i);
            };
            addAndMakeVisible (b);
        }
        store.setClickingTogglesState (true);
        store.getProperties().set ("capPx", 8.0f);
        store.setTooltip ("Arm, then click a number to store the current chord into that snapshot");
        addAndMakeVisible (store);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const int w = (r.getWidth() - 66 - 8 * 4) / 8;
        for (auto* b : buttons) { b->setBounds (r.removeFromLeft (w)); r.removeFromLeft (4); }
        store.setBounds (r.removeFromRight (62));
    }
    PluginProcessor& proc;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::TextButton store { "STORE" };
};

/** Six interval offset chips (Int1..Int6), dimmed beyond the interval count. */
struct OffsetChips : public juce::Component, private juce::Timer
{
    explicit OffsetChips (PluginProcessor& p) : proc (p)
    {
        for (int k = 0; k < 6; ++k) { auto* s = chips.add (new ParamStepper (p, Int1 + k)); addAndMakeVisible (s); }
        startTimerHz (10);
        timerCallback();
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const int w = (r.getWidth() - 5 * 4) / 6;
        for (auto* c : chips) { c->setBounds (r.removeFromLeft (w)); r.removeFromLeft (4); }
    }
    void timerCallback() override
    {
        const int n = (int) proc.value (IntCount);
        for (int k = 0; k < 6; ++k) chips[k]->setEnabled (k < n);
    }
    PluginProcessor& proc;
    juce::OwnedArray<ParamStepper> chips;
};

/** Live text cell (MIDI status). */
struct StatusText : public juce::Component, private juce::Timer
{
    StatusText (std::function<juce::String()> f) : fn (std::move (f)) { startTimerHz (4); setInterceptsMouseClicks (false, false); }
    void timerCallback() override { const auto t = fn(); if (t != text) { text = t; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::textMuted);
        g.setFont (theme::capFont (8.2f, 0));
        g.drawFittedText (text.isEmpty() ? fn() : text, getLocalBounds(), juce::Justification::centredLeft, 2, 0.9f);
    }
    std::function<juce::String()> fn;
    juce::String text;
};
} // namespace

// ============================================================================================ DetailControl
DetailControl::DetailControl (PluginProcessor& p, int paramIndex, const juce::String& lab, const juce::String& hlp, Style style, juce::StringArray segs)
    : help (hlp.isNotEmpty() ? hlp : firstSentence (paramInfo (paramIndex).help)), proc (&p), index (paramIndex), label (lab)
{
    const auto& info = paramInfo (index);
    if (style == Style::Auto)
    {
        if (info.kind == Kind::Bool) style = Style::Segments;
        else if (info.kind == Kind::Choice) style = paramChoices (index).size() <= 3 ? Style::Segments : Style::Combo;
        else if (info.kind == Kind::Int && info.maxV - info.minV <= 16) style = Style::Stepper;
        else style = Style::Slider;
    }
    switch (style)
    {
        case Style::Segments:
        {
            if (segs.isEmpty())
            {
                if (info.kind == Kind::Bool) segs = { "OFF", "ON" };
                else for (auto& c : paramChoices (index)) segs.add (juce::String (c).toUpperCase());
            }
            std::vector<float> vals;
            for (int i = 0; i < segs.size(); ++i) vals.push_back ((float) i);
            segments = std::make_unique<Segmented> (&p, index, segs, vals);
            segments->setFontHeight (7.6f);
            segments->getProperties().set ("radius", 5.0f);
            addAndMakeVisible (*segments);
            break;
        }
        case Style::Combo:
            combo = std::make_unique<ParamCombo> (p, index);
            addAndMakeVisible (*combo);
            break;
        case Style::Stepper:
            stepper = std::make_unique<ParamStepper> (p, index);
            addAndMakeVisible (*stepper);
            break;
        default:
        {
            slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, info.id, *slider);
            if (auto* prm = p.param (index)) slider->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
            slider->setVelocityModeParameters (0.35, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
            slider->setTitle (lab);
            addAndMakeVisible (*slider);
            value = std::make_unique<juce::Label>();
            value->setEditable (false, true, false);
            value->setJustificationType (juce::Justification::centredRight);
            value->setFont (theme::capFont (kValueCap, 0));
            value->setBorderSize ({ 0, 0, 0, 0 });
            value->setColour (juce::Label::textColourId, theme::text);
            value->setColour (juce::Label::backgroundWhenEditingColourId, theme::valueBox);
            value->setColour (juce::Label::outlineWhenEditingColourId, theme::accent);
            value->setTooltip ("Double-click to type a value");
            value->onTextChange = [this] {
                if (auto* prm = proc->param (index)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getValueForText (value->getText())); prm->endChangeGesture(); }
                refresh();
            };
            addAndMakeVisible (*value);
            break;
        }
    }
    setTitle (lab);
    setDescription (help);
    setTooltip (help);
    for (auto* c : getChildren()) if (auto* tc = dynamic_cast<juce::SettableTooltipClient*> (c)) tc->setTooltip (help);
    if (value) value->setTooltip (help + "  (double-click to type)");
    refresh();
}

DetailControl::DetailControl (const juce::String& lab, const juce::String& hlp, std::unique_ptr<juce::Component> content)
    : help (hlp), label (lab), custom (std::move (content))
{
    addAndMakeVisible (*custom);
    setTitle (lab);
    setTooltip (help);
}

void DetailControl::refresh()
{
    if (enabledWhen) { const bool en = enabledWhen(); if (en != isEnabled()) setEnabled (en); }
    if (value && proc != nullptr)
        if (auto* prm = proc->param (index))
            if (! value->isBeingEdited()) value->setText (prm->getCurrentValueAsText(), juce::dontSendNotification);
}

void DetailControl::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (16);
    if (value) value->setBounds (top.removeFromRight (std::min (90, top.getWidth() / 2)));
    r.removeFromTop (3);
    const int ch = std::min (r.getHeight(), 24);
    auto c = r.withHeight (ch);
    if (slider) slider->setBounds (r.withHeight (std::min (r.getHeight(), 20)).expanded (6, 0));
    if (segments) segments->setBounds (c);
    if (combo) combo->setBounds (c);
    if (stepper) stepper->setBounds (c.withWidth (std::min (c.getWidth(), 150)));
    if (custom) custom->setBounds (label.isEmpty() ? getLocalBounds() : c);
}

void DetailControl::paint (juce::Graphics& g)
{
    if (label.isEmpty()) return;
    g.setColour (isEnabled() ? theme::textLabel : theme::textDisabled);
    g.setFont (theme::capFont (kLabelCap, 1));
    g.drawText (label, juce::Rectangle<float> (0.0f, 0.0f, (float) getWidth() - (value ? 90.0f : 0.0f), 16.0f), juce::Justification::centredLeft, true);
}

// ============================================================================================ EffectDetail
void EffectDetail::BackButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat();
    if (highlighted || down) { g.setColour (juce::Colours::black.withAlpha (down ? 0.07f : 0.04f)); g.fillRoundedRectangle (r, 5.0f); }
    // thin graph icon + "Graph"
    juce::Path p;
    const float x = 6.0f, y = r.getCentreY();
    p.startNewSubPath (x, y + 1.0f);
    for (int i = 1; i <= 12; ++i) p.lineTo (x + (float) i * 1.25f, y + 1.0f - 5.0f * std::exp (-0.25f * (float) i) * std::sin ((float) i * 1.6f));
    g.setColour (theme::text);
    g.strokePath (p, juce::PathStrokeType (1.3f));
    g.setFont (theme::capFont (8.4f, 1));
    g.drawText ("GRAPH", r.withTrimmedLeft (25.0f), juce::Justification::centredLeft, false);
}

EffectDetail::EffectDetail (PluginProcessor& p, Effect e) : proc (p), effect (e)
{
    setOpaque (false);
    setWantsKeyboardFocus (true); // Escape returns to the graph
    helpBtn.setClickingTogglesState (true);
    helpBtn.setTooltip ("Help: describe the control under the pointer");
    helpBtn.getProperties().set ("capPx", 8.6f);
    helpBtn.getProperties().set ("radius", 11.0f);
    addAndMakeVisible (helpBtn);
    backBtn.setTooltip ("Back to graph");
    backBtn.onClick = [this] { if (onBack) onBack(); };
    addAndMakeVisible (backBtn);
    setTitle (e == Effect::Delay ? "Delay settings" : (e == Effect::Reverb ? "Reverb settings" : "Harmony settings"));
    rebuild();
}

int EffectDetail::modeKey() const
{
    switch (effect)
    {
        case Effect::Delay: return (int) proc.value (DelayMode) * 2 + (proc.value (DelayMode) > 0.5f ? (int) (proc.value (IvSync) > 0.5f) : (int) (proc.value (BbdSync) > 0.5f)) * 0;
        case Effect::Reverb: return (int) proc.value (ReverbMode);
        default: return (int) proc.value (NoteSource) + 8 * (proc.value (ArpSync) > 0.5f ? 1 : 0) + (proc.isCompanion() ? 64 : 0);
    }
}

void EffectDetail::rebuild()
{
    for (auto& pg : pages) for (auto& it : pg.items) removeChildComponent (it.get());
    pages.clear();
    pageTabs.reset();
    headingExtra.reset();
    auto& P = proc;
    auto add = [this] (Page& pg, int param, const juce::String& lab, const juce::String& hlp, DetailControl::Style st = DetailControl::Style::Auto, juce::StringArray segs = {}) -> DetailControl& {
        pg.items.push_back (std::make_unique<DetailControl> (proc, param, lab, hlp, st, segs));
        return *pg.items.back();
    };
    auto addCustom = [] (Page& pg, const juce::String& lab, const juce::String& hlp, std::unique_ptr<juce::Component> c) -> DetailControl& {
        pg.items.push_back (std::make_unique<DetailControl> (lab, hlp, std::move (c)));
        return *pg.items.back();
    };
    const juce::String tailHelp = "Let the previous mode ring out, or clear its tail when you switch modes.";

    if (effect == Effect::Delay)
    {
        const int dm = juce::jlimit (0, 2, (int) P.value (DelayMode));
        const bool interval = dm == 1;
        title = dm == 2 ? "TAPE SETTINGS" : (interval ? "INTERVAL SETTINGS" : "BBD SETTINGS");
        // SYNC stays reachable while the graph (which carries the SYNC button) is hidden
        auto sync = std::make_unique<ParamToggleButton> (P, dm == 2 ? TpSync : (interval ? IvSync : BbdSync), "SYNC");
        sync->getProperties().set ("capPx", 8.4f);
        sync->getProperties().set ("radius", 5.0f);
        sync->setTooltip ("Sync the delay time to the tempo; the Time knob then sets the division");
        headingExtra = std::move (sync);
        if (dm == 2)
        {
            Page pg { "TAPE", 2, {} };
            add (pg, TpHeads, "Heads", "Which playback heads sound and feed back: head 1 = Time, head 2 = 2x, head 3 = 3x.", DetailControl::Style::Combo);
            add (pg, TpWow, "Wow & flutter", "Motor and capstan speed wobble: the repeats drift in pitch.");
            add (pg, TpSpread, "Spread", "Places the active heads across the stereo field.");
            add (pg, TpHiss, "Hiss", "Tape noise while the tape carries sound; it fades a few seconds after the input stops.");
            add (pg, ClearTailOnChange, "Tail on mode change", tailHelp, DetailControl::Style::Segments, { "LET RING", "CLEAR" });
            pages.push_back (std::move (pg));
        }
        else if (! interval)
        {
            Page pg { "BBD", 2, {} };
            add (pg, BbdMotion, "Motion", "Wow and flutter depth: slow pitch drift of the repeats.");
            add (pg, BbdStereo, "Stereo pattern", "Independent channels, repeats alternating sides, or a mono source spread by offset times.");
            add (pg, BbdRate, "Motion rate", "Speed of the wow (the flutter runs faster).");
            add (pg, BbdTimeMode, "Time change", "Smooth crossfades to a new time; Tape glides there, bending the pitch like tape.");
            add (pg, BbdNoise, "Noise", "Quiet signal-dependent texture; silent when nothing plays.");
            add (pg, ClearTailOnChange, "Tail on mode change", tailHelp, DetailControl::Style::Segments, { "LET RING", "CLEAR" });
            pages.push_back (std::move (pg));
        }
        else
        {
            Page taps { "TAPS", 3, {} };
            for (int k = 0; k < 3; ++k)
            {
                if (k == 1)
                {
                    auto lab = std::make_unique<juce::Label> (juce::String(), "on the INTERVAL knob");
                    lab->setFont (theme::capFont (7.8f, 0)); lab->setColour (juce::Label::textColourId, theme::textMuted);
                    addCustom (taps, "Tap 2 interval", "Tap 2's transposition is the INTERVAL macro knob.", std::move (lab));
                }
                else
                {
                    auto& c = add (taps, IvTap1Semi + k, "Tap " + juce::String (k + 1) + " interval", "Transposition of this tap in semitones.");
                    c.enabledWhen = [&P, k] { return (int) P.value (IvTaps) > k; };
                }
            }
            for (int k = 0; k < 3; ++k) { auto& c = add (taps, IvTap1Level + k, "Tap " + juce::String (k + 1) + " level", "Level of this tap."); c.enabledWhen = [&P, k] { return (int) P.value (IvTaps) > k; }; }
            for (int k = 0; k < 3; ++k) { auto& c = add (taps, IvTap1Pan + k, "Tap " + juce::String (k + 1) + " pan", "Stereo position of this tap."); c.enabledWhen = [&P, k] { return (int) P.value (IvTaps) > k; }; }
            pages.push_back (std::move (taps));
            Page pitch { "PITCH", 2, {} };
            add (pitch, IvPitchMode, "Pitch behaviour", "Stable keeps the timing independent of the interval; Clock lets the playback rate change pitch and fragment length together.");
            add (pitch, IvDirection, "Direction", "Forward, reversed or alternating fragments (reverse adds capture delay).");
            add (pitch, IvShiftPlace, "Shift placement", "Output shifts each repeat once; Feedback cascade shifts again on every repeat (bounded).",
                 DetailControl::Style::Segments, { "OUTPUT", "CASCADE" });
            add (pitch, IvGrain, "Grain", "Pitch-shift window (Stable) or fragment length (Clock).");
            add (pitch, IvDriver, "Pitch driver", "Fixed taps, or tap pitch follows MIDI / the arp relative to the reference note (separate from the harmony notes).",
                 DetailControl::Style::Segments, { "FIXED", "MIDI", "ARP" });
            auto& ref = add (pitch, IvRef, "Reference note", "Note meaning 'no shift' for the MIDI / Arp pitch driver.");
            ref.enabledWhen = [&P] { return (int) P.value (IvDriver) != 0; };
            pages.push_back (std::move (pitch));
            Page ch { "CHARACTER", 2, {} };
            add (ch, IvTaps, "Taps", "Number of pitch-shaped taps.");
            add (ch, IvTone, "Tone", "Low-pass in the repeat loop: each repeat gets darker.");
            add (ch, ClearTailOnChange, "Tail on mode change", tailHelp, DetailControl::Style::Segments, { "LET RING", "CLEAR" });
            pages.push_back (std::move (ch));
        }
    }
    else if (effect == Effect::Reverb)
    {
        const int rm = juce::jlimit (0, 2, (int) P.value (ReverbMode));
        const bool wash = rm == 1;
        title = rm == 2 ? "HALL SETTINGS" : (wash ? "WASH SETTINGS" : "PLATE SETTINGS");
        Page pg { rm == 2 ? "HALL" : (wash ? "WASH" : "PLATE"), 2, {} };
        if (rm == 2)
        {
            add (pg, HaEarly, "Early reflections", "Level of the first reflections from the walls: high for a clear sense of room.");
            add (pg, HaDiffusion, "Diffusion", "Controls how quickly the tail becomes dense.");
            add (pg, HaLowRatio, "Bass multiplier", "Bass decay relative to the main tail (crossover about 350 Hz).");
            add (pg, HaMotion, "Motion", "Random wander of the hall: smooths metallic ringing and adds a gentle chorus.");
            add (pg, HaRate, "Motion rate", "Sets the speed of the wander.");
        }
        else
        {
            add (pg, wash ? WaSize : PlSize, "Size", "Scales the space; changes smoothly.");
            if (! wash) add (pg, PlDiffusion, "Diffusion", "Controls how quickly the tail becomes dense.");
            add (pg, wash ? WaLowRatio : PlLowRatio, "Low decay", "Bass decay relative to the main tail.");
            add (pg, wash ? WaRate : PlRate, "Motion rate", "Sets the speed of the modulation.");
        }
        pages.push_back (std::move (pg));
        Page sh { "SHIMMER", 2, {} };
        add (sh, Shimmer, "Shimmer", "Pitch-shifted feedback: every pass through the reverb rises by the interval (octave-up shimmer).");
        auto& iv = add (sh, ShimmerPitch, "Interval", "Interval of each shimmer pass, in semitones.", DetailControl::Style::Combo);
        iv.enabledWhen = [&P] { return P.value (Shimmer) > 0.5f; };
        add (sh, Width, "Stereo width", "Width of the ambience: 0 % is mono, 150 % extra wide (mono fold-down stays intact).");
        add (sh, ClearTailOnChange, "Tail on mode change", tailHelp, DetailControl::Style::Segments, { "LET RING", "CLEAR" });
        pages.push_back (std::move (sh));
    }
    else
    {
        title = "HARMONY SETTINGS";
        const int src = (int) P.value (NoteSource);
        Page source { src == 1 ? "CHORD" : (src == 2 ? "INTERVALS" : (src == 3 ? "ARP" : "MIDI")), 4, {} };
        if (src == 1)
        {
            add (source, ChRoot, "Root", "Root of the stored chord.", DetailControl::Style::Combo);
            add (source, ChQuality, "Chord", "Chord type. Custom: click keys on the keyboard to toggle its notes.", DetailControl::Style::Combo);
            add (source, ChOctave, "Octave", "Octave of the root (3 = C3).");
            add (source, ChInversion, "Inversion", "Moves the lowest notes up an octave.");
            add (source, ChSpread, "Voicing", "Close, open (2nd note up an octave) or wide (alternate notes up).");
            auto& snaps = addCustom (source, "Snapshots", "Eight stored chords: click to recall, arm STORE to save. MIDI Program Change 0-7 recalls them.", std::make_unique<SnapshotStrip> (P));
            juce::ignoreUnused (snaps);
        }
        else if (src == 2)
        {
            add (source, IntRoot, "Root", "Stored root note for the interval set.");
            add (source, IntRefSource, "Reference", "Stored root, or the lowest held MIDI note.", DetailControl::Style::Segments, { "STORED", "MIDI" });
            add (source, IntMode, "Steps", "Chromatic semitones, or diatonic steps in the key below.");
            add (source, IntCount, "Count", "How many offsets are used.");
            auto& k = add (source, IntKey, "Key", "Key for scale steps.", DetailControl::Style::Combo);
            k.enabledWhen = [&P] { return (int) P.value (IntMode) == 1; };
            auto& sc = add (source, IntScale, "Scale", "Scale for scale steps.", DetailControl::Style::Combo);
            sc.enabledWhen = [&P] { return (int) P.value (IntMode) == 1; };
            addCustom (source, "Offsets", "Interval offsets from the root (semitones or scale steps).", std::make_unique<OffsetChips> (P));
        }
        else if (src == 3)
        {
            add (source, ArpMode, "Pattern", "Order of the arpeggiated notes (Random is repeatable).", DetailControl::Style::Combo);
            add (source, ArpSync, "Timing", "Tempo division, or a free rate in steps per second.", DetailControl::Style::Segments, { "FREE", "SYNC" });
            if (P.value (ArpSync) > 0.5f) add (source, ArpRate, "Rate", "Step length as a tempo division.", DetailControl::Style::Combo);
            else add (source, ArpFreeRate, "Rate", "Steps per second.");
            add (source, ArpOctaves, "Octaves", "Octave span of the pattern.");
            add (source, ArpGate, "Gate", "Note length as a share of the step.");
            add (source, ArpSwing, "Swing", "Delays every second step.");
            add (source, ArpVelVar, "Velocity variation", "Repeatable per-step velocity variation.");
        }
        else
        {
            if (! P.isCompanion())
            {
                add (source, ChordWindow, "Chord window", "Notes arriving this close together after a latch replacement join the same chord.");
                add (source, PitchBendRange, "Bend range", "Pitch bend range for the harmony voices.");
                add (source, ModWheelTarget, "Mod wheel", "What the mod wheel adds to.", DetailControl::Style::Combo);
            }
            addCustom (source, "MIDI", "MIDI input status. Device and channel: header gear > MIDI / Setup.",
                       std::make_unique<StatusText> ([&P] { return midiStatusFor (P); }));
        }
        pages.push_back (std::move (source));
        Page voice { "VOICE", 4, {} };
        add (voice, Polyphony, "Voices", "Maximum harmony voices; extra notes steal a quiet or the oldest voice.");
        add (voice, NoteAttack, "Note attack", "Fade-in of new notes.");
        add (voice, NoteRelease, "Note release", "How notes fade after release (not the room decay).");
        add (voice, TransitionMode, "Chord change", "Crossfade between chords, or glide each voice to the nearest new note.");
        auto& apply = add (voice, ApplyHarmonyTo, "Harmonise", "Parallel routing: which branch is harmonised.", DetailControl::Style::Segments, { "BOTH", "DELAY", "REVERB" });
        apply.enabledWhen = [&P] { return (int) P.value (Routing) == 0; };
        add (voice, ClCarrier, "Carrier", "Soft (saw), Bright (saw + pulse) or Hollow (square) voice.");
        add (voice, ClDetune, "Detune", "Adds a second, detuned oscillator per note.");
        add (voice, ClStereoLink, "Stereo link", "Off keeps left/right movement; On uses one shared envelope.");
        pages.push_back (std::move (voice));
        Page voc { "VOCODER", 4, {} };
        add (voc, ClBands, "Bands", "Number of vocoder bands: more bands sound finer and more intelligible.");
        add (voc, ClAttack, "Envelope attack", "How fast the bands follow the ambience: short for articulation, long for smooth tails.");
        add (voc, ClRelease, "Envelope release", "How slowly the bands let go.");
        add (voc, ClFormant, "Formant", "Moves the tone colour up or down; the played notes stay the same.");
        add (voc, ClNoise, "Sibilance", "Adds back high consonant noise from the ambience.");
        pages.push_back (std::move (voc));
    }

    if (pages.size() > 1)
    {
        juce::StringArray names;
        for (auto& pg : pages) names.add (pg.name);
        pageTabs = std::make_unique<Segmented> (nullptr, -1, names);
        pageTabs->setFontHeight (7.6f);
        pageTabs->getProperties().set ("radius", 5.0f);
        pageTabs->onSelect = [this] (int i) { showPage (i); };
        pageTabs->setTitle (title + " pages");
        addAndMakeVisible (*pageTabs);
    }
    if (headingExtra) addAndMakeVisible (*headingExtra);
    for (auto& pg : pages) for (auto& it : pg.items) addChildComponent (*it);
    builtKey = modeKey();
    current = juce::jlimit (0, (int) pages.size() - 1, current);
    showPage (current);
}

void EffectDetail::showPage (int page)
{
    current = juce::jlimit (0, (int) pages.size() - 1, page);
    for (int i = 0; i < (int) pages.size(); ++i)
        for (auto& it : pages[(size_t) i].items) it->setVisible (i == current);
    if (pageTabs) pageTabs->setSelected (current, false);
    resized();
    repaint();
}

void EffectDetail::update()
{
    if (modeKey() != builtKey) { current = effect == Effect::Harmony ? current : 0; rebuild(); }
    juce::String h;
    if (helpBtn.getToggleState())
    {
        h = "Point at a control for help";
        if (! pages.empty())
            for (auto& it : pages[(size_t) current].items)
                if (it->isVisible() && it->isMouseOver (true)) { h = it->help; break; }
    }
    for (auto& it : pages[(size_t) current].items) it->refresh();
    if (h != helpLine) { helpLine = h; repaint (0, 0, getWidth(), 40); }
}

void EffectDetail::resized()
{
    auto r = getLocalBounds().reduced (14, 0);
    const bool compact = getHeight() < 180;
    auto head = r.removeFromTop (compact ? 38 : 40).withTrimmedTop (compact ? 8 : 10);
    backBtn.setBounds (head.removeFromRight (66));
    head.removeFromRight (6);
    helpBtn.setBounds (head.removeFromRight (22).withSizeKeepingCentre (22, 22));
    head.removeFromRight (10);
    if (pageTabs) { pageTabs->setBounds (head.removeFromRight (std::min (270, 82 * (int) pages.size())).withSizeKeepingCentre (std::min (270, 82 * (int) pages.size()), 24)); head.removeFromRight (10); }
    if (headingExtra) { headingExtra->setBounds (head.removeFromRight (58).withSizeKeepingCentre (58, 24)); head.removeFromRight (10); }
    r.removeFromTop (compact ? 4 : 8);
    r.removeFromBottom (compact ? 10 : 14);
    if (pages.empty()) return;
    auto& pg = pages[(size_t) current];
    const int cols = pg.columns, gapX = cols > 2 ? 16 : 24, gapY = 8;
    const int rowH = compact ? 44 : 46;
    const int colW = (r.getWidth() - gapX * (cols - 1)) / cols;
    int i = 0;
    for (auto& it : pg.items)
    {
        const int span = it->getTitle() == "Snapshots" ? 3 : (it->getTitle() == "Offsets" ? 2 : 1);
        if (i % cols + span > cols) i += cols - i % cols;
        const int row = i / cols, col = i % cols;
        it->setBounds (r.getX() + col * (colW + gapX), r.getY() + row * (rowH + gapY), colW * span + gapX * (span - 1), rowH);
        i += span;
    }
}

void EffectDetail::paint (juce::Graphics& g)
{
    // inset warm-ivory surface with a fine warm border (replaces the graph; no frozen plot behind it)
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    juce::Path p; p.addRoundedRectangle (r, 9.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffece5da), 0, r.getY(), juce::Colour (0xfff3eee6), 0, r.getBottom(), false));
    g.fillPath (p);
    g.setColour (juce::Colour (0x22000000));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 9.0f, r.getRight() - 9.0f);
    g.setColour (juce::Colour (0xffcfc6b7));
    g.strokePath (p, juce::PathStrokeType (1.0f));
    const bool compact = getHeight() < 180;
    auto head = getLocalBounds().reduced (14, 0).removeFromTop (compact ? 38 : 40).withTrimmedTop (compact ? 8 : 10).toFloat();
    g.setColour (theme::text);
    const auto f = theme::capFont (9.8f, 2, 0.04f);
    g.setFont (f);
    g.drawText (title, head, juce::Justification::centredLeft, false);
    if (helpLine.isNotEmpty())
    {
        const float tw = juce::GlyphArrangement::getStringWidth (f, title) + 18.0f;
        float right = (float) backBtn.getX() - 40.0f;
        if (pageTabs) right = (float) pageTabs->getX() - 12.0f;
        if (headingExtra) right = (float) headingExtra->getX() - 12.0f;
        g.setColour (theme::textMuted);
        g.setFont (theme::capFont (7.8f, 0));
        g.drawFittedText (helpLine, head.withLeft (head.getX() + tw).withRight (right).toNearestInt(), juce::Justification::centredLeft, 2, 0.85f);
    }
}

// ============================================================================================ SettingsPanel
SettingsPanel::SettingsPanel (PluginProcessor& p, std::function<void()> onClose, std::function<void()> onSaveExp)
    : proc (p), closeFn (std::move (onClose)), saveExperimentFn (std::move (onSaveExp))
{
    setTitle ("Settings");
    setWantsKeyboardFocus (true);
    tabs.setFontHeight (7.8f);
    tabs.getProperties().set ("radius", 5.0f);
    tabs.onSelect = [this] (int i) { showPage (i); };
    addAndMakeVisible (tabs);
    closeBtn.onClick = [this] { if (closeFn) closeFn(); };
    closeBtn.getProperties().set ("stroke", 1.6f);
    addAndMakeVisible (closeBtn);
    setSize (kW, kH);
    showPage (0);
    startTimerHz (6);
}

SettingsPanel::~SettingsPanel() { stopTimer(); }

void SettingsPanel::showPage (int pg)
{
    current = juce::jlimit (0, 2, pg);
    tabs.setSelected (current, false);
    proc.uiAdvancedTab = juce::StringArray { "MIDI", "Audio", "Support" }[current];
    build();
}

void SettingsPanel::build()
{
    for (auto& it : items) removeChildComponent (it.get());
    items.clear();
    for (auto* b : buttons) removeChildComponent (b);
    buttons.clear();
    status.reset(); diag.reset();
    auto& P = proc;
    auto add = [this] (int param, const juce::String& lab, const juce::String& hlp, DetailControl::Style st = DetailControl::Style::Auto, juce::StringArray segs = {}) -> DetailControl& {
        items.push_back (std::make_unique<DetailControl> (proc, param, lab, hlp, st, segs));
        addAndMakeVisible (*items.back());
        return *items.back();
    };
    auto button = [this] (const juce::String& t, std::function<void()> fn, const juce::String& tip) {
        auto* b = buttons.add (new juce::TextButton (t));
        b->onClick = std::move (fn); b->setTooltip (tip);
        b->getProperties().set ("capPx", 7.8f); b->getProperties().set ("radius", 5.0f);
        addAndMakeVisible (b);
    };
    status = std::make_unique<juce::Label>();
    status->setFont (theme::capFont (8.2f, 0));
    status->setColour (juce::Label::textColourId, theme::textMuted);
    status->setJustificationType (juce::Justification::topLeft);
    status->setMinimumHorizontalScale (0.9f);
    addAndMakeVisible (*status);

    if (current == 0)
    {
        if (! P.isCompanion()) add (MidiChannel, "MIDI channel", "Channel the harmony listens to (Omni = all). On-screen keys always play.");
        add (InputSource, "Audio input", "Which inputs feed the effect. Logic MIDI-controlled AU: the audio arrives on the side chain.", DetailControl::Style::Combo);
        add (RefTuning, "Reference tuning", "Frequency of A4 for all notes.");
        add (Tempo, "Internal tempo", "Used when the host gives no tempo, and in the standalone app.");
        add (PitchBendRange, "Bend range", "Pitch bend range for the harmony voices (MIDI and the on-screen wheel).");
        add (ModWheelTarget, "Mod wheel", "What the mod wheel adds to.", DetailControl::Style::Combo);
        if (StandaloneHost::instance() != nullptr)
            button ("AUDIO / MIDI DEVICES...", [] { if (auto* h = StandaloneHost::instance()) h->showAudioMidiSettings(); }, "Audio device, sample rate, buffer size, channels and MIDI inputs");
        button ("PANIC", [&P] { P.panic(); }, "Release all notes (latched, held, arp)");
        button ("TAIL KILL", [&P] { P.tailKill(); }, "Release notes and clear all wet tails");
    }
    else if (current == 1)
    {
        add (WetLevel, "Output (wet level)", "Level of the ambience return. Never changes the dry signal.");
        add (DryLevel, "Dry level", "Level of the untouched source.");
        add (Timing, "Timing", "Live: immediate dry. Studio: dry and wet aligned, latency reported to the host (applied when the transport stops).",
             DetailControl::Style::Segments, { "LIVE", "STUDIO" });
        add (Quality, "Quality", "Eco / Standard / High: CPU versus detail (vocoder filter order, Wash density). Changing it clears the wet tail.");
        add (WetLowCut, "Wet low cut", "High-pass on the ambience only.");
        add (WetHighCut, "Wet high cut", "Low-pass on the ambience only.");
        add (DuckAttack, "Duck attack", "How fast the ambience ducks while you play.");
        add (DuckRelease, "Duck release", "How fast the ambience returns after you stop.");
        auto& send = add (SerialSend, "Series send", "Series routing: how much of the first effect feeds the second.");
        send.enabledWhen = [&P] { return (int) P.value (Routing) != 0; };
        add (FreezeTarget, "Freeze holds", "What FREEZE holds: the reverb, the delay, or both.", DetailControl::Style::Segments, { "REVERB", "DELAY", "BOTH" });
        add (FreezeOverdub, "Freeze overdub", "Lets new input enter a frozen effect at a bounded level.");
        add (WetTrim, "A/B wet trim", "Wet trim used by the A/B loudness match (bounded to +/-12 dB).");
        button ("COPY A > B", [&P] { P.presets().copyAB (0, 1); }, "Copy the complete A state into B");
        button ("COPY B > A", [&P] { P.presets().copyAB (1, 0); }, "Copy the complete B state into A");
        button ("MATCH B", [&P] { juce::String msg; P.presets().matchLoudness (msg); juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Loudness match", msg); },
                "Match B's loudness to A: trims B's wet level so its measured loudness matches A");
        button ("A/B CLEARS TAIL", [this, &P] { P.clearTailOnAB = ! P.clearTailOnAB; buttons.getLast()->setToggleState (P.clearTailOnAB, juce::dontSendNotification); },
                "When on (orange), switching A/B cuts the ringing tail; off lets it ring into the other slot");
        buttons.getLast()->setToggleState (P.clearTailOnAB, juce::dontSendNotification);
    }
    else
    {
        diag = std::make_unique<juce::TextEditor>();
        diag->setMultiLine (true);
        diag->setReadOnly (true);
        diag->setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.5f, juce::Font::plain));
        diag->setColour (juce::TextEditor::backgroundColourId, theme::valueBox);
        addAndMakeVisible (*diag);
        button ("COPY DIAGNOSTICS", [&P] { juce::SystemClipboard::copyTextToClipboard (juce::JSON::toString (P.diagnostics())); }, "Copy the diagnostics (JSON) to the clipboard");
        button ("EXPORT...", [this] {
            auto chooser = std::make_shared<juce::FileChooser> ("Export diagnostics", juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("PlayableAmbience-diagnostics.json"), "*.json");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser] (const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f != juce::File()) f.replaceWithText (juce::JSON::toString (proc.diagnostics()));
            }); }, "Save the diagnostics as a JSON file (no audio, no personal paths)");
        button ("SAVE EXPERIMENT...", [this] { if (saveExperimentFn) saveExperimentFn(); }, "Save preset, settings, recorded MIDI and diagnostics into a folder for feedback");
    }
    timerCallback();
    resized();
    repaint();
}

void SettingsPanel::timerCallback()
{
    for (auto& it : items) it->refresh();
    juce::String s;
    if (current == 0) s = midiStatusFor (proc) + (StandaloneHost::instance() != nullptr ? "\nDevices: " + StandaloneHost::instance()->deviceSummary() : juce::String());
    else if (current == 1)
    {
        s = "Latency reported " + juce::String (proc.reportedLatency()) + " samples (Studio at this quality: " + juce::String (proc.studioLatencyFor ((int) proc.value (Quality))) + ").";
        if (proc.latencyChangePending()) s << " Stop the transport to apply.";
        s << "  A/B wet loudness: A " << juce::String (proc.presets().slotLoudnessDb (0), 1) << " dB, B " << juce::String (proc.presets().slotLoudnessDb (1), 1) << " dB.";
    }
    else s = "Read-only status. Nothing is uploaded; export writes JSON without audio or personal paths.";
    if (status && status->getText() != s) status->setText (s, juce::dontSendNotification);
    if (diag)
    {
        auto v = proc.diagnostics();
        juce::String t;
        if (auto* o = v.getDynamicObject())
            for (auto& kv : o->getProperties())
                t << kv.name.toString().paddedRight (' ', 28) << (kv.value.isObject() || kv.value.isArray() ? juce::JSON::toString (kv.value, true) : kv.value.toString()) << "\n";
        if (diag->getText() != t) diag->setText (t, false);
    }
}

void SettingsPanel::paint (juce::Graphics& g)
{
    paintCard (g, getLocalBounds().toFloat().reduced (6.0f), 12.0f);
    g.setColour (theme::text);
    g.setFont (theme::capFont (10.2f, 3, 0.04f));
    g.drawText ("SETTINGS", juce::Rectangle<float> (22.0f, 18.0f, 200.0f, 22.0f), juce::Justification::centredLeft, false);
}

void SettingsPanel::resized()
{
    auto r = getLocalBounds().reduced (6).reduced (16, 12);
    auto head = r.removeFromTop (28);
    closeBtn.setBounds (head.removeFromRight (28).withSizeKeepingCentre (26, 26));
    r.removeFromTop (8);
    tabs.setBounds (r.removeFromTop (26));
    r.removeFromTop (12);
    if (status) status->setBounds (r.removeFromTop (current == 1 ? 30 : 30));
    r.removeFromTop (6);
    if (! buttons.isEmpty())
    {
        auto br = r.removeFromBottom (28);
        const int perRow = (int) buttons.size();
        const int bw = (br.getWidth() - 8 * (perRow - 1)) / perRow;
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setBounds (br.getX() + (i % perRow) * (bw + 8), br.getY() + (i / perRow) * 34, bw, 28);
        r.removeFromBottom (10);
    }
    if (diag) { diag->setBounds (r); return; }
    const int cols = 2, gapX = 16, rowH = 42, gapY = 6;
    const int colW = (r.getWidth() - gapX) / cols;
    for (int i = 0; i < (int) items.size(); ++i)
        items[(size_t) i]->setBounds (r.getX() + (i % cols) * (colW + gapX), r.getY() + (i / cols) * (rowH + gapY), colW, rowH);
}
} // namespace pa
