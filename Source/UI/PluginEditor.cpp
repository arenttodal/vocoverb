#include "PluginEditor.h"

#include "BuildInfo.h"
#include "Presets/PresetManager.h"
#include "StandalonePanel.h"

namespace pa
{
namespace
{
/** Draws `text` with its capital letters starting at canvas y `capTop` (baseline = capTop + cap height). */
void capText (juce::Graphics& g, const juce::String& text, const juce::Font& f, float x, float capTop, float capPx, juce::Justification j = juce::Justification::left, float width = 0.0f)
{
    g.setFont (f);
    if (j == juce::Justification::left) { g.drawSingleLineText (text, (int) std::lround (x), (int) std::lround (capTop + capPx)); return; }
    const float w = juce::GlyphArrangement::getStringWidth (f, text);
    const float x0 = j == juce::Justification::horizontallyCentred ? x + (width - w) * 0.5f : x + width - w;
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, text, x0, capTop + capPx);
    ga.draw (g);
}

/** Letter spacing (kerning factor) that makes `text` exactly `targetW` wide in font `f`. */
float trackingToFit (juce::Font f, const juce::String& text, float targetW)
{
    const float w0 = juce::GlyphArrangement::getStringWidth (f.withExtraKerningFactor (0.0f), text);
    const int gaps = std::max (1, text.length() - 1);
    return (targetW - w0) / ((float) gaps * f.getHeight());
}

/** Text button with an icon at its left (ADVANCED). */
class IconTextButton : public juce::TextButton
{
public:
    IconTextButton (const juce::String& t, IconButton::Icon i) : juce::TextButton (t), icon (i) {}
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        getLookAndFeel().drawButtonBackground (g, *this, {}, highlighted, down);
        const auto c = isEnabled() ? theme::text : theme::textDisabled;
        // 15.5 px icon, 7.5 px gap, label; the pair is centred in the button
        const auto f = theme::capFont ((float) getProperties().getWithDefault ("capPx", 8.55), 1);
        constexpr float iconW = 15.5f, gapW = 7.5f;
        const float tw = juce::GlyphArrangement::getStringWidth (f, getButtonText());
        const float x0 = ((float) getWidth() - (iconW + gapW + tw)) * 0.5f;
        IconButton::drawIcon (g, icon, juce::Rectangle<float> (iconW, 14.0f).withCentre ({ x0 + iconW * 0.5f, (float) getHeight() * 0.5f }), c, false, 1.4f);
        g.setColour (c);
        g.setFont (f);
        g.drawText (getButtonText(), juce::Rectangle<float> (x0 + iconW + gapW, 0.0f, tw + 4.0f, (float) getHeight()), juce::Justification::centredLeft, false);
    }

private:
    IconButton::Icon icon;
};

/** Transparent text button (preset name). */
class PlainTextButton : public juce::Button
{
public:
    PlainTextButton() : juce::Button ("Preset") {}
    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        if (highlighted) { g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillRect (getLocalBounds()); }
        g.setColour (theme::text);
        capText (g, getButtonText(), theme::capFont (13.5f * theme::kCapPerEm, 1), 7.0f, (float) getHeight() * 0.5f - 5.5f, 10.5f, juce::Justification::horizontallyCentred, (float) getWidth());
    }
};

/** C MINOR badge: shows the voiced chord; opens the source settings (chord / intervals / arp editing). */
class ChordBadge : public juce::Button
{
public:
    ChordBadge() : juce::Button ("Chord") { setTooltip ("Voiced chord. Click to edit the stored chord, intervals or arpeggiator"); }
    juce::String text;
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        paintField (g, r, 6.0f, true, highlighted);
        if (down) { g.setColour (juce::Colours::black.withAlpha (0.06f)); g.fillRoundedRectangle (r, 6.0f); }
        g.setColour (isEnabled() ? theme::text : theme::textDisabled);
        const auto f = theme::capFont (10.0f, 1, 0.06f);
        capText (g, text, f, 0.0f, r.getCentreY() - 5.0f, 10.0f, juce::Justification::horizontallyCentred, (float) getWidth());
    }
};

/** Source-specific settings shown in a call-out from the chord badge (all former source-bar controls). */
class SourceSettings : public juce::Component
{
public:
    explicit SourceSettings (PluginProcessor& p)
        : proc (p), chRoot (p, ChRoot), chQuality (p, ChQuality), chSpread (p, ChSpread), intRef (p, IntRefSource), intMode (p, IntMode), intKey (p, IntKey),
          intScale (p, IntScale), arpMode (p, ArpMode), arpRate (p, ArpRate), chOct (p, ChOctave), chInv (p, ChInversion), intRoot (p, IntRoot), intCount (p, IntCount),
          arpOct (p, ArpOctaves), arpSync (p, ArpSync, "SYNC"), policy (&p, NoNotePolicy, { "HOLD LAST", "RELEASE", "AMBIENT" })
    {
        src = (int) p.value (NoteSource);
        intRoot.prefix = "ROOT"; chOct.prefix = "OCT"; chInv.prefix = "INV"; intCount.prefix = "COUNT"; arpOct.prefix = "OCT";
        for (int i = 0; i < 8; ++i)
        {
            snap[i].setButtonText (juce::String (i + 1));
            snap[i].setTooltip ("Chord snapshot " + juce::String (i + 1) + " (click to recall; arm STORE first; MIDI Program Change " + juce::String (i) + ")");
            snap[i].onClick = [this, i] {
                if (storeArm.getToggleState()) { proc.storeSnapshot (i); storeArm.setToggleState (false, juce::dontSendNotification); }
                else proc.recallSnapshot (i);
            };
        }
        storeArm.setClickingTogglesState (true);
        storeArm.setTooltip ("Arm, then click a snapshot number to store the current chord into it");
        arpGate.setSliderStyle (juce::Slider::LinearHorizontal);
        arpGate.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        arpGateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramInfo (ArpGate).id, arpGate);
        arpGate.setTooltip ("Arp gate length");
        switch (src)
        {
            case 1: for (juce::Component* c : std::initializer_list<juce::Component*> { &chRoot, &chQuality, &chOct, &chInv, &chSpread, &storeArm }) addAndMakeVisible (c);
                    for (auto& s : snap) addAndMakeVisible (s);
                    title = "STORED CHORD"; break;
            case 2: for (juce::Component* c : std::initializer_list<juce::Component*> { &intRoot, &intRef, &intMode, &intKey, &intScale, &intCount }) addAndMakeVisible (c);
                    title = "INTERVALS  (offsets: Advanced > MIDI)"; break;
            case 3: for (juce::Component* c : std::initializer_list<juce::Component*> { &arpMode, &arpRate, &arpSync, &arpOct, &arpGate }) addAndMakeVisible (c);
                    title = "ARPEGGIATOR"; break;
            default: addAndMakeVisible (policy); title = "MIDI  -  what happens when no keys are held"; break;
        }
        setSize (600, 96);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::textMuted);
        g.setFont (theme::capFont (8.5f, 1, 0.08f));
        g.drawText (title, getLocalBounds().removeFromTop (24).reduced (12, 0), juce::Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 0).withTrimmedTop (26);
        auto row = r.removeFromTop (30);
        auto place = [&row] (juce::Component& c, int w) { c.setBounds (row.removeFromLeft (std::min (w, row.getWidth())).reduced (0, 1)); row.removeFromLeft (6); };
        switch (src)
        {
            case 1:
                place (chRoot, 70); place (chQuality, 100); place (chOct, 92); place (chInv, 86); place (chSpread, 92);
                row = r.withTrimmedTop (6).removeFromTop (28);
                for (auto& s : snap) place (s, 30);
                place (storeArm, 66);
                break;
            case 2: place (intRoot, 112); place (intRef, 120); place (intMode, 104); place (intKey, 60); place (intScale, 150); break;
            case 3: place (arpMode, 110); place (arpRate, 76); place (arpSync, 60); place (arpOct, 90); place (arpGate, 150); break;
            default: place (policy, 330); break;
        }
        if (src == 2) { row = r.withTrimmedTop (6).removeFromTop (28); place (intCount, 104); }
    }

private:
    PluginProcessor& proc;
    int src = 0;
    juce::String title;
    ParamCombo chRoot, chQuality, chSpread, intRef, intMode, intKey, intScale, arpMode, arpRate;
    ParamStepper chOct, chInv, intRoot, intCount, arpOct;
    ParamToggleButton arpSync;
    Segmented policy;
    juce::TextButton snap[8], storeArm { "STORE" };
    juce::Slider arpGate;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arpGateAttach;
};
} // namespace

// ======================================================================================== MainView
class MainView : public juce::Component
{
public:
    MainView (PluginProcessor& p, PluginEditor& e);
    void paint (juce::Graphics&) override;
    void resized() override;
    void update();                    // ~30 Hz
    void refreshPresetName();
    void setReferenceFixture (bool on) { fixtureMode = on; dGraph.setReferenceFixture (on); rGraph.setReferenceFixture (on); hGraph.setReferenceFixture (on); }

    LevelMeter inMeter { "IN" }, outMeter { "OUT" };

private:
    void showPresetMenu();
    void savePresetDialog();
    void bindModeKnobs();
    void paintStatic (juce::Graphics& g);
    juce::String midiStatusText() const;

    PluginProcessor& proc;
    PluginEditor& editor;
    CaptureClock clock;
    bool fixtureMode = false;

    // header
    IconButton prevPreset { IconButton::Icon::Prev, "Previous preset" }, nextPreset { IconButton::Icon::Next, "Next preset" },
        heart { IconButton::Icon::Heart, "Favourite this preset" }, loadBtn { IconButton::Icon::Folder, "Load a preset file" },
        saveBtn { IconButton::Icon::Save, "Save user preset" }, gearBtn { IconButton::Icon::Gear, "Settings: mix, output and dry levels, timing, quality, diagnostics" };
    PlainTextButton presetName;
    Segmented abSeg { nullptr, -1, { "A", "B" } };
    ParamKnob mixKnob;

    // delay
    EnableDot dDot;
    ParamCombo dMode;
    IconButton dMenu { IconButton::Icon::Kebab, "Delay settings (all BBD / Interval parameters, freeze)" };
    DelayView dGraph;
    ParamToggleButton bbdSync, ivSync;
    std::unique_ptr<ParamKnob> dk[5];

    // reverb
    EnableDot rDot;
    ParamCombo rMode;
    IconButton rMenu { IconButton::Icon::Kebab, "Reverb settings (all Plate / Wash parameters)" };
    ReverbView rGraph;
    std::unique_ptr<ParamKnob> rk[5];

    // routing
    ChoiceIconButton routeParallel, routeDR, routeRD;
    Segmented placement;

    // harmony
    EnableDot hDot;
    ChordBadge chordBadge;
    ParamCombo source;
    ChoiceIconButton polHold, polRelease, polAmbient;
    IconButton hMenu { IconButton::Icon::Kebab, "Harmony settings (Classic bands, envelopes, carrier, notes, MIDI)" };
    HarmonyView hGraph;
    std::unique_ptr<ParamKnob> hk[4];
    juce::Label hint;

    // performance row
    Wheel pitchWheel, modWheel;
    HarmonyKeyboard keyboard;
    ParamToggleButton latch, freeze, wetOnly;
    IconTextButton advancedBtn { "ADVANCED", IconButton::Icon::Sliders };
    IconButton panic { IconButton::Icon::Panic, "Panic: release all notes (latched, held, arp)" },
        tailKill { IconButton::Icon::TailKill, "Tail Kill: release notes and clear all wet tails (short fade, dry untouched)" };

    juce::Image staticLayer;
    float staticScale = -1.0f;
    int lastModeKey = -1, lastHostNoteCount = 0;
    juce::uint32 lastHostNoteTicks = 0;
};

MainView::MainView (PluginProcessor& p, PluginEditor& e)
    : proc (p), editor (e),
      mixKnob (p, Mix, "DRY / WET", ParamKnob::Style::Header),
      dDot (p, DelayEnable, "delay"), dMode (p, DelayMode), dGraph (p, clock),
      bbdSync (p, BbdSync, "SYNC"), ivSync (p, IvSync, "SYNC"),
      rDot (p, ReverbEnable, "reverb"), rMode (p, ReverbMode), rGraph (p, clock),
      routeParallel (p, Routing, 0, IconButton::Icon::RouteParallel, "Parallel", "Parallel: delay and reverb both hear the input (two independent branches)"),
      routeDR (p, Routing, 1, IconButton::Icon::RouteDR, juce::String (juce::CharPointer_UTF8 ("Delay \xe2\x86\x92 Reverb")), juce::String (juce::CharPointer_UTF8 ("Delay \xe2\x86\x92 Reverb: the delay feeds the reverb"))),
      routeRD (p, Routing, 2, IconButton::Icon::RouteRD, juce::String (juce::CharPointer_UTF8 ("Reverb \xe2\x86\x92 Delay")), juce::String (juce::CharPointer_UTF8 ("Reverb \xe2\x86\x92 Delay: the reverb feeds the delay"))),
      placement (&p, Placement, { "AFTER SPACE", "BEFORE SPACE" }),
      hDot (p, HarmEnable, "harmony"),
      source (p, NoteSource),
      polHold (p, NoNotePolicy, 0, IconButton::Icon::Pin, "Hold Last", "Hold Last: with no keys held, the last chord keeps sounding"),
      polRelease (p, NoNotePolicy, 1, IconButton::Icon::Wave, "Release", "Release: with no keys held, the voices fade out (Note Release)"),
      polAmbient (p, NoNotePolicy, 2, IconButton::Icon::Cloud, "Ambient", "Ambient: with no keys held, the wet returns to ordinary ambience"),
      hGraph (p, clock),
      pitchWheel (p, true), modWheel (p, false), keyboard (p),
      latch (p, Latch, "LATCH"), freeze (p, Freeze, "FREEZE"), wetOnly (p, WetOnly, "WET ONLY")
{
    setOpaque (true);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &prevPreset, &nextPreset, &heart, &loadBtn, &saveBtn, &gearBtn, &presetName, &abSeg, &mixKnob,
                                &inMeter, &outMeter, &dDot, &dMode, &dMenu, &dGraph, &bbdSync, &ivSync, &rDot, &rMode, &rMenu, &rGraph,
                                &routeParallel, &routeDR, &routeRD, &placement, &hDot, &chordBadge, &source, &polHold, &polRelease, &polAmbient, &hMenu, &hGraph, &hint,
                                &pitchWheel, &modWheel, &keyboard, &latch, &freeze, &wetOnly, &advancedBtn, &panic, &tailKill })
        addAndMakeVisible (c);

    const int dInit[5] = { BbdTime, BbdFeedback, BbdTone, BbdAge, BbdLevel };
    const char* dNames[5] = { "Time", "Feedback", "Tone", "Age", "Level" };
    for (int i = 0; i < 5; ++i) { dk[i] = std::make_unique<ParamKnob> (p, dInit[i], dNames[i], ParamKnob::Style::Engine); addAndMakeVisible (*dk[i]); }
    const int rInit[5] = { WaDecay, WaBloom, WaTone, WaMotion, WaLevel };
    const char* rNames[5] = { "Decay", "Bloom", "Tone", "Motion", "Level" };
    for (int i = 0; i < 5; ++i) { rk[i] = std::make_unique<ParamKnob> (p, rInit[i], rNames[i], ParamKnob::Style::Engine); addAndMakeVisible (*rk[i]); }
    const int hInit[4] = { Depth, Colour, Transition, DuckAmount };
    const char* hNames[4] = { "Depth", "Colour", "Transition", "Duck" };
    for (int i = 0; i < 4; ++i) { hk[i] = std::make_unique<ParamKnob> (p, hInit[i], hNames[i], ParamKnob::Style::Engine); hk[i]->getProperties().set ("boxDy", -5); addAndMakeVisible (*hk[i]); }

    prevPreset.onClick = [this] { proc.presets().step (-1); refreshPresetName(); };
    nextPreset.onClick = [this] { proc.presets().step (1); refreshPresetName(); };
    heart.onClick = [this] { proc.presets().toggleFavourite (proc.presets().currentName()); refreshPresetName(); };
    presetName.onClick = [this] { showPresetMenu(); };
    presetName.setTooltip ("Preset browser: factory presets (read-only) and your user presets");
    loadBtn.onClick = [this] {
        auto ch = std::make_shared<juce::FileChooser> ("Load preset", PresetManager::userPresetDirectory(), "*.papreset");
        ch->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, ch] (const juce::FileChooser& fc) {
            juce::String err;
            if (fc.getResult().existsAsFile() && ! proc.presets().loadFile (fc.getResult(), err))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Load preset", err);
            refreshPresetName();
        });
    };
    saveBtn.onClick = [this] { savePresetDialog(); };
    gearBtn.onClick = [this] { editor.openAdvanced ("Mix / Timing"); };
    for (auto* b : { &loadBtn, &saveBtn, &gearBtn }) b->getProperties().set ("stroke", 1.7f);
    for (auto* b : { &prevPreset, &nextPreset }) { b->getProperties().set ("iconSize", 22.0f); b->getProperties().set ("stroke", 2.0f); }
    heart.getProperties().set ("stroke", 1.8f); heart.getProperties().set ("iconSize", 21.0f);
    loadBtn.getProperties().set ("iconSize", 20.0f); saveBtn.getProperties().set ("iconSize", 19.0f); gearBtn.getProperties().set ("iconSize", 20.0f);
    abSeg.onSelect = [this] (int s) { proc.presets().switchAB (s); refreshPresetName(); };
    abSeg.setTooltip ("A/B comparison of complete states (Settings > Mix / Timing for copy, loudness match, clear tail)");
    abSeg.setTitle ("A/B slot");
    abSeg.setSelected (proc.presets().abSlot(), false);
    abSeg.setFontHeight (10.0f);
    mixKnob.slider.setTooltip ("DRY / WET: overall blend. 50% keeps dry and wet at their full levels; 0% = dry only, 100% = wet only. "
                               "Output (wet) and dry trims are in Settings > Mix / Timing.");

    dMenu.onClick = [this] { editor.openAdvanced ("Delay"); };
    rMenu.onClick = [this] { editor.openAdvanced ("Reverb"); };
    hMenu.onClick = [this] { editor.openAdvanced ("Harmony"); };
    for (auto* m : { &dMenu, &rMenu, &hMenu }) m->getProperties().set ("stroke", 1.6f);
    for (auto* c : { &dMode, &rMode }) c->getProperties().set ("textInset", 17);
    source.getProperties().set ("textInset", 14);
    advancedBtn.onClick = [this] { editor.openAdvanced (proc.uiAdvancedTab); };
    advancedBtn.setTooltip ("All settings with help text: Classic details, MIDI, arp, timing, A/B, diagnostics");
    panic.onClick = [this] { proc.panic(); };
    tailKill.onClick = [this] { proc.tailKill(); };
    panic.setTitle ("Panic: all notes off"); tailKill.setTitle ("Tail kill: clear wet tails");
    for (auto* b : { &panic, &tailKill }) { b->getProperties().set ("stroke", 1.7f); b->getProperties().set ("iconSize", 22.0f); }
    latch.setTooltip ("LATCH holds the played harmony after key-up (harmony state). Separate from FREEZE and from the Hold Last policy.");
    freeze.setTooltip ("FREEZE holds the audio ambience (never creates sound by itself). Chords can still change over it in After Space.");
    wetOnly.setTooltip ("WET ONLY removes the dry signal completely (return tracks). It overrides DRY / WET and keeps its value.");
    for (juce::Button* b : std::initializer_list<juce::Button*> { &latch, &freeze, &wetOnly, &advancedBtn, &bbdSync, &ivSync })
        b->getProperties().set ("radius", 6.0f);
    bbdSync.getProperties().set ("capPx", 9.5f); ivSync.getProperties().set ("capPx", 9.5f);
    // compact action group: 11.75 px medium labels, identical radius / padding for all four
    for (juce::Button* b : std::initializer_list<juce::Button*> { &latch, &freeze, &wetOnly, &advancedBtn })
    {
        b->getProperties().set ("capPx", 11.75 * theme::kCapPerEm);
        b->getProperties().set ("radius", 5.0f);
    }
    for (auto* b : { &routeParallel, &routeDR, &routeRD })
    {
        b->getProperties().set ("iconBox", true);
        b->getProperties().set ("stroke", 1.6f);
    }
    routeParallel.getProperties().set ("iconW", 74.0f); routeParallel.getProperties().set ("iconH", 34.0f);
    for (auto* b : { &routeDR, &routeRD }) { b->getProperties().set ("iconW", 84.0f); b->getProperties().set ("iconH", 26.0f); }
    for (auto* b : { &polHold, &polRelease, &polAmbient }) { b->getProperties().set ("iconBox", true); b->getProperties().set ("iconW", 24.0f); b->getProperties().set ("iconH", 24.0f); b->getProperties().set ("stroke", 1.8f); }
    placement.setFontHeight (9.0f);
    placement.getProperties().set ("firstFraction", 183.0 / 379.0); // AFTER SPACE | BEFORE SPACE split at x 1177
    chordBadge.onClick = [this] {
        auto content = std::make_unique<SourceSettings> (proc);
        juce::CallOutBox::launchAsynchronously (std::move (content), chordBadge.getScreenBounds(), nullptr);
    };
    hint.setColour (juce::Label::textColourId, theme::displayLabel.withAlpha (0.8f));
    hint.setFont (theme::capFont (8.0f, 1, 0.04f));
    hint.setJustificationType (juce::Justification::centredRight);
    hint.setInterceptsMouseClicks (false, false);
    keyboard.setRange (36, 29); // C2 .. C6
    refreshPresetName();
    bindModeKnobs();
}

void MainView::refreshPresetName()
{
    const auto n = proc.presets().currentName();
    presetName.setButtonText (n);
    heart.setFilled (proc.presets().isFavourite (n));
    abSeg.setSelected (proc.presets().abSlot(), false);
    repaint();
}

void MainView::showPresetMenu()
{
    auto& pm = proc.presets();
    pm.rescan();
    juce::PopupMenu menu;
    juce::String lastCat;
    juce::PopupMenu sub;
    const auto& es = pm.entries();
    std::map<juce::String, juce::PopupMenu> cats;
    juce::StringArray order;
    for (int i = 0; i < es.size(); ++i)
    {
        const auto& e = es.getReference (i);
        const auto cat = e.factory ? e.category : juce::String ("User");
        if (! order.contains (cat)) order.add (cat);
        juce::String label = e.name;
        if (pm.isFavourite (e.name)) label = juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xa5 ")) + label;
        cats[cat].addItem (label, true, e.name == pm.currentName(), [this, i] { proc.presets().loadEntry (i); refreshPresetName(); });
    }
    for (auto& c : order) menu.addSubMenu (c, cats[c]);
    menu.addSeparator();
    menu.addItem ("Save as user preset...", [this] { savePresetDialog(); });
    menu.addItem ("Reveal user preset folder", [] { PresetManager::userPresetDirectory().createDirectory(); PresetManager::userPresetDirectory().revealToUser(); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetName));
}

void MainView::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name for the user preset:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.presets().currentName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r) {
        if (r != 1) return;
        const auto name = w->getTextEditorContents ("name");
        juce::String err;
        if (proc.presets().saveUser (name, PresetManager::Collision::Ask, err)) { refreshPresetName(); return; }
        if (err != "exists") { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset", err); return; }
        auto* c = new juce::AlertWindow ("Preset exists", "A user preset named \"" + name + "\" already exists.", juce::MessageBoxIconType::QuestionIcon);
        c->addButton ("Replace", 1); c->addButton ("Keep Both", 2); c->addButton ("Cancel", 0);
        c->enterModalState (true, juce::ModalCallbackFunction::create ([this, name] (int choice) {
            if (choice == 0) return;
            juce::String e2;
            proc.presets().saveUser (name, choice == 1 ? PresetManager::Collision::Replace : PresetManager::Collision::KeepBoth, e2);
            refreshPresetName();
        }), true);
    }), true);
}


void MainView::bindModeKnobs()
{
    const bool interval = proc.value (DelayMode) > 0.5f;
    if (interval)
    {
        dk[0]->bind (proc.value (IvSync) > 0.5f ? IvDiv : IvTime, "Time");
        dk[1]->bind (IvFeedback, "Feedback");
        dk[2]->bind (IvTap2Semi, "Interval");
        dk[3]->bind (IvSmear, "Smear");
        dk[4]->bind (IvLevel, "Level");
    }
    else
    {
        dk[0]->bind (proc.value (BbdSync) > 0.5f ? BbdDiv : BbdTime, "Time");
        dk[1]->bind (BbdFeedback, "Feedback");
        dk[2]->bind (BbdTone, "Tone");
        dk[3]->bind (BbdAge, "Age");
        dk[4]->bind (BbdLevel, "Level");
    }
    bbdSync.setVisible (! interval);
    ivSync.setVisible (interval);
    const bool wash = proc.value (ReverbMode) > 0.5f;
    if (wash) { rk[0]->bind (WaDecay, "Decay"); rk[1]->bind (WaBloom, "Bloom"); rk[2]->bind (WaTone, "Tone"); rk[3]->bind (WaMotion, "Motion"); rk[4]->bind (WaLevel, "Level"); }
    else { rk[0]->bind (PlDecay, "Decay"); rk[1]->bind (PlPredelay, "Pre-delay"); rk[2]->bind (PlTone, "Tone"); rk[3]->bind (PlMotion, "Motion"); rk[4]->bind (PlLevel, "Level"); }
}

void MainView::resized()
{
    // ---- header (canvas coordinates of the approved reference)
    // ---- header: everything centred on theme::kHeaderMidY (62)
    const int hy = (int) theme::kHeaderMidY;
    prevPreset.setBounds (527, hy - 20, 50, 40);
    presetName.setBounds (577, hy - 20, 298, 40);
    heart.setBounds (875, hy - 20, 52, 40);
    nextPreset.setBounds (927, hy - 20, 51, 40);
    loadBtn.setBounds (juce::Rectangle<int> (32, 32).withCentre ({ 1070, hy }));
    saveBtn.setBounds (juce::Rectangle<int> (32, 32).withCentre ({ 1118, hy }));
    gearBtn.setBounds (juce::Rectangle<int> (32, 32).withCentre ({ 1167, hy }));
    abSeg.setBounds (1204, hy - 15, 79, 29);
    inMeter.setBounds (1305, hy - 34, 36, 68);
    outMeter.setBounds (1342, hy - 34, 37, 68);
    // compact Dry/Wet utility: 44 px face, 54 px tick ring, two-line caption; group centred on the header line
    mixKnob.setBounds (1450 - ParamKnob::kHeaderCx, hy - 38, ParamKnob::kHeaderW, ParamKnob::kHeaderH);
    // ---- delay / reverb
    const int ey = (int) theme::kEngineHeaderY, hhy = (int) theme::kHarmonyHeaderY;
    dDot.setBounds (36, ey - 12, 25, 24);                 // 13 px visible dot, centre x 48.5; hit area stops before the title
    dMode.setBounds (556, ey - 16, 156, 32);
    dMenu.setBounds (juce::Rectangle<int> (28, 32).withCentre ({ 736, ey }));
    dGraph.setBounds (42, 170, 704, 215);
    bbdSync.setBounds (670, 182, 61, 28);
    ivSync.setBounds (670, 182, 61, 28);
    const int dX[5] = { 105, 245, 388, 532, 675 }, rX[5] = { 858, 1002, 1143, 1288, 1433 }, hX[4] = { 1024, 1169, 1298, 1433 };
    for (int i = 0; i < 5; ++i) dk[i]->setBounds (dX[i] - ParamKnob::kEngineCx, 459 - ParamKnob::kEngineCy, ParamKnob::kEngineW, ParamKnob::kEngineH);
    rDot.setBounds (36 + 750, ey - 12, 25, 24);           // same offset from the reverb graph (x 791) as the delay dot
    rMode.setBounds (1298, ey - 16, 163, 32);
    rMenu.setBounds (juce::Rectangle<int> (28, 32).withCentre ({ 1486, ey }));
    rGraph.setBounds (791, 170, 704, 215);
    for (int i = 0; i < 5; ++i) rk[i]->setBounds (rX[i] - ParamKnob::kEngineCx, 459 - ParamKnob::kEngineCy, ParamKnob::kEngineW, ParamKnob::kEngineH);
    // ---- routing strip
    routeParallel.setBounds (255, 572, 136, 43);
    routeDR.setBounds (411, 572, 144, 43);
    routeRD.setBounds (576, 572, 145, 43);
    placement.setBounds (994, 574, 379, 39);
    // ---- harmony
    hDot.setBounds (36, hhy - 12, 25, 24);
    chordBadge.setBounds (825, hhy - 16, 105, 32);
    source.setBounds (1084, hhy - 16, 132, 32);
    polHold.setBounds (1232, hhy - 16, 62, 32);
    polRelease.setBounds (1302, hhy - 16, 64, 32);
    polAmbient.setBounds (1374, hhy - 16, 65, 32);
    hMenu.setBounds (juce::Rectangle<int> (28, 32).withCentre ({ 1486, hhy }));
    hGraph.setBounds (42, 686, 912, 160);
    hint.setBounds (520, 690, 426, 18);
    for (int i = 0; i < 4; ++i) hk[i]->setBounds (hX[i] - ParamKnob::kEngineCx, 768 - ParamKnob::kEngineCy, ParamKnob::kEngineW, ParamKnob::kEngineH);
    // ---- performance row
    pitchWheel.setBounds (62 - 23, 877, 46, 100);
    modWheel.setBounds (111 - 23, 877, 46, 100);
    keyboard.setBounds (150, 872, 968, 111);
    // compact 2 x 2 action group (110 x 33, 8 px gaps), centred between the divider (x 1134) and the utility column
    {
        constexpr int bw = 110, bh = 33, gap = theme::kGap;
        const int gx = (1134 + 1460) / 2 - (2 * bw + gap) / 2, gy = 866 + (121 - (2 * bh + gap)) / 2;
        latch.setBounds (gx, gy, bw, bh);
        freeze.setBounds (gx + bw + gap, gy, bw, bh);
        wetOnly.setBounds (gx, gy + bh + gap, bw, bh);
        advancedBtn.setBounds (gx + bw + gap, gy + bh + gap, bw, bh);
        // utility icons share the button rows' centre lines
        panic.setBounds (juce::Rectangle<int> (32, 32).withCentre ({ 1477, gy + bh / 2 }));
        tailKill.setBounds (juce::Rectangle<int> (32, 32).withCentre ({ 1477, gy + bh + gap + bh / 2 }));
    }
    staticScale = -1.0f;
}

void MainView::paint (juce::Graphics& g)
{
    const float scale = std::max (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (std::abs (scale - staticScale) > 0.01f || ! staticLayer.isValid())
    {
        staticLayer = juce::Image (juce::Image::ARGB, (int) std::ceil (theme::kCanvasW * scale), (int) std::ceil (theme::kCanvasH * scale), true);
        juce::Graphics sg (staticLayer);
        sg.addTransform (juce::AffineTransform::scale (scale));
        paintStatic (sg);
        staticScale = scale;
    }
    g.drawImage (staticLayer, juce::Rectangle<float> (0, 0, (float) theme::kCanvasW, (float) theme::kCanvasH), juce::RectanglePlacement::stretchToFit);
}

void MainView::paintStatic (juce::Graphics& g)
{
    // backdrop and warm shell (slight spatial variation, never flat white)
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffece7df), 0, 0, juce::Colour (0xffd8cfc2), 0, (float) theme::kCanvasH, false));
    g.fillAll();
    const juce::Rectangle<float> shell (15.0f, 15.0f, 1506.0f, 983.0f);
    {
        juce::Path p; p.addRoundedRectangle (shell, 15.0f);
        juce::DropShadow (juce::Colour (0x26402810), 10, { 0, 3 }).drawForPath (g, p);
        juce::ColourGradient sg (theme::shellTop, 0, shell.getY(), theme::shellBottom, 0, shell.getBottom(), false);
        sg.addColour (0.12, juce::Colour (0xfff3ede4));
        g.setGradientFill (sg);
        g.fillPath (p);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.20f), shell.getCentreX(), shell.getY(), juce::Colours::white.withAlpha (0.0f),
                                                 shell.getCentreX(), shell.getY() + 420.0f, true));
        g.fillPath (p);
        g.setColour (juce::Colour (0x18604830));
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }
    // cards
    paintCard (g, { 22.0f, 118.0f, 743.0f, 436.0f });
    paintCard (g, { 772.0f, 118.0f, 743.0f, 436.0f });
    paintCard (g, { 22.0f, 563.0f, 1493.0f, 63.0f }, 12.0f);
    paintCard (g, { 22.0f, 634.0f, 1493.0f, 225.0f });
    paintCard (g, { 22.0f, 866.0f, 1493.0f, 121.0f });

    // brand: one group (28 px title + 9.75 px subtitle, 7 px apart) centred on the header line y 62
    {
        const float titleCap = 28.0f * theme::kCapPerEm, subCap = 9.75f * theme::kCapPerEm, gap = 7.0f;
        const float top = theme::kHeaderMidY - 0.5f * (titleCap + gap + subCap);
        const auto heavy = theme::capFont (titleCap, 3), regular = theme::capFont (titleCap, 0);
        const juce::String a ("PLAYABLE"), b ("AMBIENCE");
        g.setColour (theme::text);
        juce::GlyphArrangement ga;
        ga.addLineOfText (heavy, a, 46.0f, top + titleCap);
        ga.addLineOfText (regular, b, 46.0f + juce::GlyphArrangement::getStringWidth (heavy, a) + titleCap * 0.42f, top + titleCap);
        ga.draw (g);
        auto sub = theme::capFont (subCap, 0);
        sub = sub.withExtraKerningFactor (2.75f / sub.getHeight()); // ~2.75 px tracking
        g.setColour (theme::textMuted);
        capText (g, "VOCODED DELAY & REVERB", sub, 47.0f, top + titleCap + gap, subCap);
    }
    // preset bar: 40 px high, same segments, centred on the header line
    {
        const juce::Rectangle<float> bar (527.0f, theme::kHeaderMidY - 20.0f, 451.0f, 40.0f);
        paintField (g, bar, 7.0f, true);
        paintField (g, { 577.5f, bar.getY() + 1.0f, 297.0f, 38.0f }, 4.0f, true);
        g.setColour (juce::Colour (0xffd5ccbd));
        for (float x : { 577.0f, 875.0f, 927.0f }) g.fillRect (juce::Rectangle<float> (x - 0.5f, bar.getY() + 1.0f, 1.0f, bar.getHeight() - 2.0f));
    }
    // dividers
    g.setColour (juce::Colour (0xffd6cdbf));
    // dividers: equal insets within their strip / header line
    g.fillRect (juce::Rectangle<float> (1293.5f, theme::kHeaderMidY - 18.0f, 1.0f, 36.0f));
    g.fillRect (juce::Rectangle<float> (795.0f, 575.0f, 1.0f, 39.0f));   // routing strip 563..626: 12 px insets
    g.fillRect (juce::Rectangle<float> (1133.0f, 878.0f, 1.0f, 97.0f));  // performance card 866..987: 12 px insets
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.fillRect (juce::Rectangle<float> (1294.5f, theme::kHeaderMidY - 18.0f, 1.0f, 36.0f));
    g.fillRect (juce::Rectangle<float> (796.0f, 575.0f, 1.0f, 39.0f));
    g.fillRect (juce::Rectangle<float> (1134.0f, 878.0f, 1.0f, 97.0f));

    // section titles and strip labels
    g.setColour (theme::text);
    // section headings: one size/weight, visible cap centred on the card's header line, 10 px after the dot
    {
        const float cap = theme::kSectionTitleEm * theme::kCapPerEm;
        const auto f = theme::capFont (cap, 3, 0.01f);
        capText (g, "DELAY", f, theme::kSectionTitleX, theme::kEngineHeaderY - cap * 0.5f, cap);
        capText (g, "REVERB", f, 772.0f - 22.0f + theme::kSectionTitleX - 1.0f, theme::kEngineHeaderY - cap * 0.5f, cap);
        capText (g, "HARMONY", f, theme::kSectionTitleX, theme::kHarmonyHeaderY - cap * 0.5f, cap);
    }
    g.setColour (theme::textLabel);
    capText (g, "ROUTING", theme::capFont (10.0f, 1, 0.07f), 163.0f, 588.0f, 10.0f);
    capText (g, "PLACEMENT", theme::capFont (10.0f, 1, 0.07f), 871.0f, 588.0f, 10.0f);
    capText (g, "SOURCE", theme::capFont (9.3f, 1, 0.0f), 1016.0f, theme::kHarmonyHeaderY - 4.65f, 9.3f);
}

juce::String MainView::midiStatusText() const
{
    auto& t = proc.telemetry();
    const int notesIn = t.hostNoteOnCounter.load(), msgs = t.hostMidiCounter.load(), dropped = t.hostFilteredCounter.load();
    if (proc.isCompanion()) return "Audio AU: this plug-in type receives no MIDI. Use Chord, Intervals, Arp or the on-screen keys.";
    if (notesIn > 0)
        return "MIDI in: ch " + juce::String (t.lastHostChannel.load()) + ", last " + juce::String (paramValueToText (ShRef, (float) t.lastHostNote.load())) + " (" + juce::String (notesIn) + " notes)";
    if (dropped > 0) return "MIDI on ch " + juce::String (t.lastHostChannel.load()) + " is dropped by the channel filter (Advanced > MIDI)";
    if (msgs > 0) return "MIDI in: " + juce::String (msgs) + " messages, no notes yet";
    if (proc.isStandalone()) return "No MIDI yet: enable your controller in Audio / MIDI (on-screen keys always work)";
    if (proc.wrapperType == juce::AudioProcessor::wrapperType_AudioUnit) return "No MIDI yet. Logic: Side Chain setup. Ableton Live: use the VST3";
    return "No MIDI yet. Live: MIDI track > MIDI To > this track > Playable Ambience";
}

void MainView::update()
{
    auto& t = proc.telemetry();
    inMeter.setLevels (t.inPeakL.exchange (0.0f), t.inPeakR.exchange (0.0f));
    outMeter.setLevels (t.outPeakL.exchange (0.0f), t.outPeakR.exchange (0.0f));
    clock.update (t, proc.currentSampleRate());
    const int modeKey = (int) proc.value (DelayMode) + 2 * (int) proc.value (ReverbMode) + 4 * (proc.value (BbdSync) > 0.5f) + 8 * (proc.value (IvSync) > 0.5f);
    if (modeKey != lastModeKey) { lastModeKey = modeKey; bindModeKnobs(); }
    if (! fixtureMode)
    {
        dGraph.update();
        rGraph.update();
        hGraph.update();
    }
    keyboard.update();
    for (auto& k : dk) k->refreshValue();
    for (auto& k : rk) k->refreshValue();
    for (auto& k : hk) k->refreshValue();
    mixKnob.refreshValue();
    dDot.refresh(); rDot.refresh(); hDot.refresh();
    // enable states: processing stays bound; disabled sections are dimmed
    const bool dOn = proc.value (DelayEnable) > 0.5f, rOn = proc.value (ReverbEnable) > 0.5f, hOn = effectiveHarmonyMethod (proc.currentParams()) != 0;
    dGraph.setDimmed (! dOn, "DELAY OFF - CLICK THE DOT TO ENABLE");
    rGraph.setDimmed (! rOn, "REVERB OFF - CLICK THE DOT TO ENABLE");
    hGraph.setDimmed (! hOn, "HARMONY OFF - ORDINARY AMBIENCE");
    for (auto& k : dk) k->setAlpha (dOn ? 1.0f : 0.5f);
    for (auto& k : rk) k->setAlpha (rOn ? 1.0f : 0.5f);
    for (int i = 0; i < 3; ++i) hk[i]->setEnabled (hOn);
    placement.setEnabled (hOn);
    const bool wo = proc.value (WetOnly) > 0.5f;
    mixKnob.setAlpha (wo ? 0.55f : 1.0f);
    mixKnob.slider.setTooltip (wo ? "WET ONLY is on: it overrides DRY / WET (dry removed, wet at full level). The stored blend returns when Wet Only is off."
                                  : "DRY / WET: overall blend. 50% keeps dry and wet at their full levels; 0% = dry only, 100% = wet only. Output (wet) and dry trims: Settings > Mix / Timing.");
    // chord badge: the voiced chord (or the stored chord for the Chord source)
    {
        std::vector<int> voiced;
        for (int v = 0; v < kMaxVoices; ++v)
        {
            const int n = t.voiceNote[(size_t) v].load();
            if (n >= 0 && t.voiceGate[(size_t) v].load() && std::find (voiced.begin(), voiced.end(), n) == voiced.end()) voiced.push_back (n);
        }
        juce::String txt = chordName (voiced);
        const int src = (int) proc.value (NoteSource);
        if (txt.isEmpty()) txt = src == 3 ? "ARP" : (src == 2 ? "INTERVALS" : "NO NOTES");
        if (txt != chordBadge.text) { chordBadge.text = txt; chordBadge.repaint(); }
    }
    // source tooltip carries the live MIDI status (no permanent diagnostics in the layout)
    source.setTooltip (juce::String (paramInfo (NoteSource).help) + "\n" + midiStatusText());
    // transient hints inside the harmony well (only when something needs attention)
    juce::String h;
    if (proc.latencyChangePending()) h = "STOP TRANSPORT TO APPLY TIMING / QUALITY";
    const int hn = t.hostNoteOnCounter.load();
    if (hn != lastHostNoteCount) { lastHostNoteCount = hn; lastHostNoteTicks = juce::Time::getMillisecondCounter(); }
    const bool recent = hn > 0 && juce::Time::getMillisecondCounter() - lastHostNoteTicks < 4000;
    const int srcNow = (int) proc.value (NoteSource);
    if (recent && (srcNow == 1 || (srcNow == 2 && (int) proc.value (IntRefSource) == 0)))
        h = juce::String ("MIDI NOTES ARRIVING - SOURCE IS ") + (srcNow == 1 ? "CHORD" : "INTERVALS (STORED ROOT)");
    if (t.protectionActive.exchange (false)) h = "WET LIMITER ACTIVE";
    if (fixtureMode) h = {};
    if (hint.getText() != h) hint.setText (h, juce::dontSendNotification);
    // A/B loudness tracking
    const float wl = proc.wetRmsDb();
    if (wl > -80.0f) proc.presets().noteLoudness (wl);
    if (abSeg.selected() != proc.presets().abSlot()) abSeg.setSelected (proc.presets().abSlot(), false);
    if (presetName.getButtonText() != proc.presets().currentName()) refreshPresetName();
}

// ======================================================================================== editor
PluginEditor::PluginEditor (PluginProcessor& p) : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    view = std::make_unique<MainView> (p, *this);
    addAndMakeVisible (canvas);
    canvas.addAndMakeVisible (*view);
    int top = 0;
    if (p.isStandalone() && p.source() != nullptr)
    {
        standalone = std::make_unique<StandalonePanel> (p);
        standalone->onSaveExperiment = [this] { saveExperiment(); };
        canvas.addAndMakeVisible (*standalone);
        standalone->setBounds (14, 8, theme::kCanvasW - 28, kStripH - 8);
        top = kStripH;
    }
    view->setBounds (0, top, theme::kCanvasW, theme::kCanvasH);
    canvas.setSize (theme::kCanvasW, theme::kCanvasH + top);
    proc.presets().onChange = [this] { juce::Component::SafePointer<PluginEditor> sp (this); juce::MessageManager::callAsync ([sp] { if (sp) sp->view->refreshPresetName(); }); };
    setResizable (true, true);
    const double aspect = (double) canvas.getWidth() / (double) canvas.getHeight();
    setResizeLimits ((int) (canvas.getWidth() * kMinScale), (int) (canvas.getHeight() * kMinScale), (int) (canvas.getWidth() * kMaxScale), (int) (canvas.getHeight() * kMaxScale));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (aspect);
    setSize ((int) std::lround (canvas.getWidth() * kDefaultScale), (int) std::lround (canvas.getHeight() * kDefaultScale));
    setWantsKeyboardFocus (false);
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    proc.presets().onChange = nullptr;
    advanced.reset();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

float PluginEditor::currentScale() const
{
    return juce::jlimit (0.2f, 4.0f, (float) getWidth() / (float) canvas.getWidth());
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::backdrop);
}

void PluginEditor::resized()
{
    // one uniform scale for the whole design (no reflow, no independent knob sizing)
    const float s = std::min ((float) getWidth() / (float) canvas.getWidth(), (float) getHeight() / (float) canvas.getHeight());
    const float ox = ((float) getWidth() - (float) canvas.getWidth() * s) * 0.5f, oy = ((float) getHeight() - (float) canvas.getHeight() * s) * 0.5f;
    canvas.setTopLeftPosition (0, 0);
    canvas.setTransform (juce::AffineTransform::scale (s).translated (ox, oy));
    if (advanced) advanced->setBounds (getLocalBounds());
}

void PluginEditor::timerCallback()
{
    if (isShowing() || frame < 3) view->update();
    ++frame;
}

void PluginEditor::tickForSnapshot()
{
    view->update(); // headless captures: the editor is not on screen, so drive the 30 Hz update directly
    ++frame;
}

void PluginEditor::refreshForSnapshot()
{
    resized();
    for (int i = 0; i < 3; ++i) view->update();
}

void PluginEditor::setReferenceFixture (bool on)
{
    view->setReferenceFixture (on);
    view->update();
}

juce::Component* PluginEditor::mainView() const { return view.get(); }

std::unique_ptr<juce::Component> PluginEditor::createSourceSettingsForSnapshot()
{
    auto c = std::make_unique<SourceSettings> (proc);
    c->setLookAndFeel (&lnf);
    return c;
}

juce::Image PluginEditor::snapshotCanvas (float scale)
{
    return view->createComponentSnapshot (view->getLocalBounds(), true, scale);
}

void PluginEditor::showAdvancedForSnapshot (const juce::String& tab)
{
    if (tab.isEmpty()) closeAdvanced(); else openAdvanced (tab);
}

void PluginEditor::openAdvanced (const juce::String& tab)
{
    if (! advanced)
    {
        focusBeforeAdvanced = juce::Component::getCurrentlyFocusedComponent();
        advanced = std::make_unique<AdvancedPanel> (proc, [this] { juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PluginEditor> (this)] { if (sp) sp->closeAdvanced(); }); });
        advanced->onSaveExperiment = [this] { saveExperiment(); };
        addAndMakeVisible (*advanced);
        advanced->setBounds (getLocalBounds());
    }
    advanced->showTab (tab);
    advanced->grabKeyboardFocus();
}

void PluginEditor::closeAdvanced()
{
    advanced.reset();
    if (focusBeforeAdvanced != nullptr && focusBeforeAdvanced->isShowing()) focusBeforeAdvanced->grabKeyboardFocus();
}

bool PluginEditor::keyPressed (const juce::KeyPress& k)
{
    if (k.isKeyCode (juce::KeyPress::escapeKey) && advanced) { closeAdvanced(); return true; }
    return false;
}

bool PluginEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".papreset" || (standalone && (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac"))) return true;
    }
    return false;
}

void PluginEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& path : files)
    {
        juce::File f (path);
        if (f.hasFileExtension ("papreset")) { juce::String err; if (! proc.presets().loadFile (f, err)) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset", err); }
        else if (standalone) standalone->loadAudioFile (f);
    }
}

void PluginEditor::saveExperiment()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a folder for the experiment", juce::File::getSpecialLocation (juce::File::userDesktopDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc) {
        auto dir = fc.getResult();
        if (dir == juce::File()) return;
        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
        auto folder = dir.getChildFile ("PlayableAmbience-Experiment-" + stamp);
        folder.createDirectory();
        folder.getChildFile ("preset.papreset").replaceWithText (juce::JSON::toString (proc.presets().toJson (proc.presets().currentName())));
        folder.getChildFile ("diagnostics.json").replaceWithText (juce::JSON::toString (proc.diagnostics()));
        auto* o = new juce::DynamicObject();
        o->setProperty ("preset", proc.presets().currentName());
        o->setProperty ("abSlot", proc.presets().abSlot() == 0 ? "A" : "B");
        o->setProperty ("created", stamp);
        if (auto* src = proc.source())
        {
            if (auto d = src->currentData()) o->setProperty ("source", d->name);
            juce::Array<juce::var> ev;
            for (auto& e : src->recordedEvents())
            {
                auto* eo = new juce::DynamicObject();
                eo->setProperty ("t", e.time); eo->setProperty ("status", (int) e.status); eo->setProperty ("d1", (int) e.d1); eo->setProperty ("d2", (int) e.d2);
                ev.add (juce::var (eo));
            }
            o->setProperty ("recordedMidi", ev);
        }
        o->setProperty ("notes", "Describe what you tried in Feedback-Template.md and attach this folder (and an exported audition WAV if useful).");
        folder.getChildFile ("experiment.json").replaceWithText (juce::JSON::toString (juce::var (o)));
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Save Experiment", "Saved to " + folder.getFullPathName());
    });
}
} // namespace pa
