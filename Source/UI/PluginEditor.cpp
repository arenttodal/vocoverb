#include "PluginEditor.h"

#include "BuildInfo.h"
#include "Presets/PresetManager.h"
#include "StandalonePanel.h"

namespace pa
{
// ======================================================================================== MainView
class MainView : public juce::Component
{
public:
    MainView (PluginProcessor& p, PluginEditor& e);
    int layout (int width);           // returns preferred height
    void paint (juce::Graphics&) override;
    void resized() override { layout (getWidth()); }
    void update();                    // ~30 Hz
    void refreshPresetName();

    LevelMeter inMeter { "IN" }, outMeter { "OUT" };

private:
    void showPresetMenu();
    void savePresetDialog();
    void layoutSourceBar (juce::Rectangle<int> r);
    void bindModeKnobs();
    void paintCardHeader (juce::Graphics& g, juce::Rectangle<int> card, const juce::String& title, const juce::String& subtitle);

    PluginProcessor& proc;
    PluginEditor& editor;

    // header
    IconButton prevPreset { IconButton::Icon::Prev, "Previous preset" }, nextPreset { IconButton::Icon::Next, "Next preset" },
        heart { IconButton::Icon::Heart, "Favourite this preset" }, loadBtn { IconButton::Icon::Folder, "Load a preset file" },
        saveBtn { IconButton::Icon::Save, "Save user preset" }, gearBtn { IconButton::Icon::Gear, "Mix, timing and quality settings" };
    juce::TextButton presetName;
    Segmented abSeg { nullptr, -1, { "A", "B" } };
    ParamKnob output;

    // delay
    EnableDot dDot;
    Segmented dMode;
    IconButton dMenu { IconButton::Icon::Kebab, "Delay advanced settings" };
    DelayGraph dGraph;
    ParamToggleButton bbdSync, ivSync;
    Segmented ivPitch, ivDir;
    std::unique_ptr<ParamKnob> dk[5];
    juce::TextButton dEnable { "ENABLE DELAY" };

    // reverb
    EnableDot rDot;
    Segmented rMode;
    IconButton rMenu { IconButton::Icon::Kebab, "Reverb advanced settings" };
    ReverbGraph rGraph;
    std::unique_ptr<ParamKnob> rk[5];
    juce::TextButton rEnable { "ENABLE REVERB" };

    // routing
    Segmented routing, placement;

    // harmony
    struct MethodDot : public juce::Component, public juce::SettableTooltipClient
    {
        PluginProcessor& proc; int lastMethod = 1;
        explicit MethodDot (PluginProcessor& p) : proc (p) { setTooltip ("Harmony on/off (Off = ordinary ambience, keeps settings)"); setTitle ("Harmony on/off"); }
        void paint (juce::Graphics& g) override
        {
            const bool on = proc.value (HarmMethod) > 0.5f;
            auto r = getLocalBounds().toFloat().withSizeKeepingCentre (13.0f, 13.0f);
            if (on) { g.setColour (theme::accent.withAlpha (0.25f)); g.fillEllipse (r.expanded (2.5f)); g.setColour (theme::accent); g.fillEllipse (r); }
            else { g.setColour (theme::border); g.fillEllipse (r); g.setColour (theme::borderDark); g.drawEllipse (r, 1.0f); }
        }
        void mouseDown (const juce::MouseEvent&) override
        {
            const int m = (int) proc.value (HarmMethod);
            if (m > 0) { lastMethod = m; proc.setValue (HarmMethod, 0.0f); }
            else proc.setValue (HarmMethod, (float) lastMethod);
        }
    } mDot;
    Segmented method, source;
    IconButton hMenu { IconButton::Icon::Kebab, "Harmony advanced settings" };
    HarmonyGraph hGraph;
    std::unique_ptr<ParamKnob> hk[4];
    // source bar
    Segmented policy;
    ParamCombo chRoot, chQuality, chSpread, intRef, intMode, intKey, intScale, arpMode, arpRate;
    ParamStepper chOct, chInv, intRoot, intCount, arpOct;
    juce::TextButton snap[8], storeArm { "STORE" };
    ParamToggleButton arpSync;
    juce::Slider arpGate;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arpGateAttach;
    juce::Rectangle<int> sourceBarArea, midiLedArea;
    juce::String sourceInfo;

    // performance row
    Wheel pitchWheel, modWheel;
    HarmonyKeyboard keyboard;
    ParamToggleButton latch, freeze, wetOnly;
    juce::TextButton advancedBtn { "ADVANCED" };
    IconButton panic { IconButton::Icon::Panic, "Panic: release all notes (latched, held, arp)" },
        tailKill { IconButton::Icon::TailKill, "Tail Kill: release notes and clear all wet tails (short fade, dry untouched)" };

    // layout rectangles
    juce::Rectangle<int> header, dCard, rCard, routeStrip, hCard, perfRow, dKnobArea, rKnobArea;
    bool dCollapsed = false, rCollapsed = false, stacked = false;
    int lastMidiCount = 0; float midiLed = 0.0f;
    juce::String statusText, dryWetText;
    int lastLayoutKey = -1;
};

MainView::MainView (PluginProcessor& p, PluginEditor& e)
    : proc (p), editor (e),
      output (p, WetLevel, "OUTPUT", true),
      dDot (p, DelayEnable, "delay"), dMode (&p, DelayMode, { "BBD", "INTERVAL" }), dGraph (p),
      bbdSync (p, BbdSync, "SYNC"), ivSync (p, IvSync, "SYNC"),
      ivPitch (&p, IvPitchMode, { "STABLE", "CLOCK" }), ivDir (&p, IvDirection, { "FWD", "REV", "ALT" }),
      rDot (p, ReverbEnable, "reverb"), rMode (&p, ReverbMode, { "PLATE", "WASH" }), rGraph (p),
      routing (&p, Routing, { "PARALLEL", juce::String (juce::CharPointer_UTF8 ("DELAY \xe2\x86\x92 REVERB")), juce::String (juce::CharPointer_UTF8 ("REVERB \xe2\x86\x92 DELAY")) }),
      placement (&p, Placement, { "AFTER SPACE", "BEFORE SPACE" }),
      mDot (p),
      method (&p, HarmMethod, { "OFF", "CLASSIC", "FFT", "RESONATOR", "SHIFT" }),
      source (&p, NoteSource, { "MIDI", "CHORD", "INTERVALS", "ARP" }),
      hGraph (p),
      policy (&p, NoNotePolicy, { "HOLD LAST", "RELEASE", "AMBIENT" }),
      chRoot (p, ChRoot), chQuality (p, ChQuality), chSpread (p, ChSpread), intRef (p, IntRefSource), intMode (p, IntMode), intKey (p, IntKey), intScale (p, IntScale),
      arpMode (p, ArpMode), arpRate (p, ArpRate),
      chOct (p, ChOctave), chInv (p, ChInversion), intRoot (p, IntRoot), intCount (p, IntCount), arpOct (p, ArpOctaves),
      arpSync (p, ArpSync, "SYNC"),
      pitchWheel (p, true), modWheel (p, false), keyboard (p),
      latch (p, Latch, "LATCH"), freeze (p, Freeze, "FREEZE"), wetOnly (p, WetOnly, "WET ONLY")
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &prevPreset, &nextPreset, &heart, &loadBtn, &saveBtn, &gearBtn, &presetName, &abSeg, &output,
                                &inMeter, &outMeter,
                                &dDot, &dMode, &dMenu, &dGraph, &bbdSync, &ivSync, &ivPitch, &ivDir, &dEnable,
                                &rDot, &rMode, &rMenu, &rGraph, &rEnable, &routing, &placement,
                                &mDot, &method, &source, &hMenu, &hGraph, &policy,
                                &chRoot, &chQuality, &chSpread, &intRef, &intMode, &intKey, &intScale, &arpMode, &arpRate,
                                &chOct, &chInv, &intRoot, &intCount, &arpOct, &storeArm, &arpSync, &arpGate,
                                &pitchWheel, &modWheel, &keyboard, &latch, &freeze, &wetOnly, &advancedBtn, &panic, &tailKill })
        addAndMakeVisible (c);

    const int dInit[5] = { BbdTime, BbdFeedback, BbdTone, BbdAge, BbdLevel };
    const char* dNames[5] = { "Time", "Feedback", "Tone", "Age", "Level" };
    for (int i = 0; i < 5; ++i) { dk[i] = std::make_unique<ParamKnob> (p, dInit[i], dNames[i]); addAndMakeVisible (*dk[i]); }
    const int rInit[5] = { WaDecay, WaBloom, WaTone, WaMotion, WaLevel };
    const char* rNames[5] = { "Decay", "Bloom", "Tone", "Motion", "Level" };
    for (int i = 0; i < 5; ++i) { rk[i] = std::make_unique<ParamKnob> (p, rInit[i], rNames[i]); addAndMakeVisible (*rk[i]); }
    const int hInit[4] = { Depth, Colour, Transition, DuckAmount };
    const char* hNames[4] = { "Depth", "Colour", "Transition", "Duck" };
    for (int i = 0; i < 4; ++i) { hk[i] = std::make_unique<ParamKnob> (p, hInit[i], hNames[i]); addAndMakeVisible (*hk[i]); }

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
    abSeg.onSelect = [this] (int s) { proc.presets().switchAB (s); refreshPresetName(); };
    abSeg.setTooltip ("A/B comparison of complete states (Advanced > Mix / Timing for copy, loudness match, clear tail)");
    abSeg.setTitle ("A/B slot");
    abSeg.setSelected (proc.presets().abSlot(), false);
    output.slider.setTooltip ("OUTPUT = wet return level (ambience only). Dry level is in Advanced > Mix / Timing and is never changed by this knob.");

    dMenu.onClick = [this] { editor.openAdvanced ("Delay"); };
    rMenu.onClick = [this] { editor.openAdvanced ("Reverb"); };
    hMenu.onClick = [this] { editor.openAdvanced ("Harmony"); };
    dEnable.onClick = [this] { proc.setValue (DelayEnable, 1.0f); };
    rEnable.onClick = [this] { proc.setValue (ReverbEnable, 1.0f); };
    advancedBtn.onClick = [this] { editor.openAdvanced (proc.uiAdvancedTab); };
    advancedBtn.setTooltip ("All settings with help text: method details, MIDI, arp, timing, A/B, diagnostics");
    panic.onClick = [this] { proc.panic(); };
    tailKill.onClick = [this] { proc.tailKill(); };
    latch.setTooltip ("LATCH holds the played harmony after key-up (harmony state). Separate from FREEZE.");
    freeze.setTooltip ("FREEZE holds the audio ambience (never creates sound by itself). Chords can still change over it in After Space.");
    wetOnly.setTooltip ("WET ONLY removes the dry signal completely (use on return tracks)");

    for (int i = 0; i < 8; ++i)
    {
        snap[i].setButtonText (juce::String (i + 1));
        snap[i].setTooltip ("Chord snapshot " + juce::String (i + 1) + " (click to recall; arm STORE first to save; MIDI Program Change " + juce::String (i) + ")");
        snap[i].onClick = [this, i] {
            if (storeArm.getToggleState()) { proc.storeSnapshot (i); storeArm.setToggleState (false, juce::dontSendNotification); }
            else proc.recallSnapshot (i);
        };
        addAndMakeVisible (snap[i]);
    }
    storeArm.setClickingTogglesState (true);
    storeArm.setTooltip ("Arm, then click a snapshot number to store the current chord into it");
    arpGate.setSliderStyle (juce::Slider::LinearHorizontal);
    arpGate.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    arpGateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramInfo (ArpGate).id, arpGate);
    arpGate.setTooltip ("Arp gate length");
    intRoot.prefix = "ROOT"; chOct.prefix = "OCT"; chInv.prefix = "INV"; intCount.prefix = "COUNT"; arpOct.prefix = "OCT";
    method.setFontHeight (11.5f);
    source.setFontHeight (11.5f);
    routing.setFontHeight (11.5f);
    placement.setFontHeight (11.5f);
    policy.setFontHeight (11.0f);
    ivPitch.setFontHeight (10.5f);
    ivDir.setFontHeight (10.5f);
    refreshPresetName();
    bindModeKnobs();
}

void MainView::refreshPresetName()
{
    const auto n = proc.presets().currentName();
    presetName.setButtonText (n);
    heart.setFilled (proc.presets().isFavourite (n));
    abSeg.setSelected (proc.presets().abSlot(), false);
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
    bbdSync.setVisible (! interval && ! dCollapsed);
    ivSync.setVisible (interval && ! dCollapsed);
    ivPitch.setVisible (interval && ! dCollapsed);
    ivDir.setVisible (interval && ! dCollapsed);
    const bool wash = proc.value (ReverbMode) > 0.5f;
    if (wash)
    {
        rk[0]->bind (WaDecay, "Decay"); rk[1]->bind (WaBloom, "Bloom"); rk[2]->bind (WaTone, "Tone"); rk[3]->bind (WaMotion, "Motion"); rk[4]->bind (WaLevel, "Level");
    }
    else
    {
        rk[0]->bind (PlDecay, "Decay"); rk[1]->bind (PlPredelay, "Pre-delay"); rk[2]->bind (PlTone, "Tone"); rk[3]->bind (PlMotion, "Motion"); rk[4]->bind (PlLevel, "Level");
    }
}

int MainView::layout (int width)
{
    const bool standaloneUnused = false; juce::ignoreUnused (standaloneUnused);
    const int m = 14;
    int y = m;
    const bool dOn = proc.value (DelayEnable) > 0.5f, rOn = proc.value (ReverbEnable) > 0.5f;
    dCollapsed = ! dOn && rOn;
    rCollapsed = ! rOn && dOn;
    stacked = width < 1120;
    header = { m, y, width - 2 * m, 70 };
    y += 70 + 6;
    const int cardH = 340;
    if (stacked)
    {
        dCard = { m, y, width - 2 * m, dCollapsed ? 60 : cardH };
        y += dCard.getHeight() + 8;
        rCard = { m, y, width - 2 * m, rCollapsed ? 60 : cardH };
        y += rCard.getHeight() + 6;
    }
    else
    {
        const int full = width - 2 * m, gap = 10;
        int dw = (full - gap) / 2;
        if (dCollapsed) dw = 150;
        else if (rCollapsed) dw = full - gap - 150;
        dCard = { m, y, dw, cardH };
        rCard = { m + dw + gap, y, full - dw - gap, cardH };
        y += cardH + 6;
    }
    routeStrip = { m, y, width - 2 * m, 42 };
    y += 42 + 6;
    hCard = { m, y, width - 2 * m, 236 };
    y += 236 + 6;
    perfRow = { m, y, width - 2 * m, 106 };
    y += 106 + m;

    // ---- header
    {
        auto h = header.reduced (10, 6);
        auto right = h.removeFromRight (360);
        output.setBounds (right.removeFromRight (78));
        right.removeFromRight (10);
        outMeter.setBounds (right.removeFromRight (34).withTrimmedTop (4).withTrimmedBottom (4));
        inMeter.setBounds (right.removeFromRight (34).withTrimmedTop (4).withTrimmedBottom (4));
        right.removeFromRight (10);
        abSeg.setBounds (right.removeFromRight (64).withSizeKeepingCentre (64, 28));
        right.removeFromRight (10);
        gearBtn.setBounds (right.removeFromRight (34).withSizeKeepingCentre (32, 32));
        right.removeFromRight (4);
        saveBtn.setBounds (right.removeFromRight (34).withSizeKeepingCentre (32, 32));
        right.removeFromRight (2);
        loadBtn.setBounds (right.removeFromRight (34).withSizeKeepingCentre (32, 32));
        const int titleW = width < 1150 ? 250 : 330;
        auto avail = h.withTrimmedLeft (titleW);
        auto mid = avail.withSizeKeepingCentre (std::min (380, avail.getWidth() - 10), 44);
        prevPreset.setBounds (mid.removeFromLeft (40));
        nextPreset.setBounds (mid.removeFromRight (40));
        heart.setBounds (mid.removeFromRight (36).reduced (4));
        presetName.setBounds (mid);
    }

    // ---- delay card
    auto cardInner = [] (juce::Rectangle<int> c) { return c.reduced (14, 10); };
    {
        auto c = cardInner (dCard);
        auto top = c.removeFromTop (34);
        dDot.setBounds (top.removeFromLeft (22));
        const bool collapsedNarrow = dCollapsed && ! stacked;
        dMenu.setBounds (top.removeFromRight (28));
        if (! collapsedNarrow && ! dCollapsed) dMode.setBounds (top.removeFromRight (std::min (260, top.getWidth() / 2)).reduced (0, 2));
        else dMode.setBounds ({});
        const bool showBody = ! dCollapsed;
        dGraph.setVisible (showBody);
        for (auto& k : dk) k->setVisible (showBody);
        dEnable.setVisible (dCollapsed);
        if (dCollapsed)
        {
            dEnable.setBounds (collapsedNarrow ? c.withSizeKeepingCentre (118, 34).withY (c.getY() + 60) : c.withSizeKeepingCentre (160, 30));
        }
        else
        {
            c.removeFromTop (6);
            auto knobs = c.removeFromBottom (118);
            dKnobArea = knobs;
            dGraph.setBounds (c.withTrimmedBottom (8));
            const int kw = knobs.getWidth() / 5;
            for (int i = 0; i < 5; ++i) dk[i]->setBounds (knobs.removeFromLeft (kw).reduced (4, 0));
            auto g = dGraph.getBounds().reduced (10, 8);
            auto pills = g.removeFromTop (22);
            bbdSync.setBounds (pills.removeFromRight (52));
            ivSync.setBounds (bbdSync.getBounds());
            pills.removeFromRight (6);
            ivDir.setBounds (pills.removeFromRight (120));
            pills.removeFromRight (6);
            ivPitch.setBounds (pills.removeFromRight (120));
        }
    }
    // ---- reverb card
    {
        auto c = cardInner (rCard);
        auto top = c.removeFromTop (34);
        rDot.setBounds (top.removeFromLeft (22));
        rMenu.setBounds (top.removeFromRight (28));
        if (! rCollapsed) rMode.setBounds (top.removeFromRight (std::min (260, top.getWidth() / 2)).reduced (0, 2));
        else rMode.setBounds ({});
        rGraph.setVisible (! rCollapsed);
        for (auto& k : rk) k->setVisible (! rCollapsed);
        rEnable.setVisible (rCollapsed);
        if (rCollapsed)
            rEnable.setBounds (! stacked ? c.withSizeKeepingCentre (118, 34).withY (c.getY() + 60) : c.withSizeKeepingCentre (160, 30));
        else
        {
            c.removeFromTop (6);
            auto knobs = c.removeFromBottom (118);
            rKnobArea = knobs;
            rGraph.setBounds (c.withTrimmedBottom (8));
            const int kw = knobs.getWidth() / 5;
            for (int i = 0; i < 5; ++i) rk[i]->setBounds (knobs.removeFromLeft (kw).reduced (4, 0));
        }
    }
    bindModeKnobs();
    // ---- routing strip
    {
        auto s = routeStrip.reduced (12, 7);
        const int half = s.getWidth() / 2;
        auto left = s.removeFromLeft (half);
        left.removeFromLeft (std::max (70, left.getWidth() / 2 - 260));
        left.removeFromLeft (70);
        routing.setBounds (left.removeFromLeft (std::min (380, left.getWidth())));
        auto right = s;
        right.removeFromLeft (std::max (90, right.getWidth() / 2 - 230));
        placement.setBounds (right.removeFromLeft (std::min (300, right.getWidth())));
    }
    // ---- harmony card
    {
        auto c = hCard.reduced (14, 10);
        auto top = c.removeFromTop (32);
        mDot.setBounds (top.removeFromLeft (22));
        hMenu.setBounds (top.removeFromRight (28));
        source.setBounds (top.removeFromRight (std::min (330, top.getWidth() / 3)).reduced (0, 2));
        top.removeFromRight (12);
        top.removeFromLeft (std::min (320, top.getWidth() / 3));
        method.setBounds (top.removeFromRight (std::min (400, top.getWidth())).reduced (0, 2));
        c.removeFromTop (6);
        auto knobs = c.removeFromRight (std::min (440, c.getWidth() / 3 + 20));
        knobs.removeFromLeft (16);
        const int kw = knobs.getWidth() / 4;
        auto kr = knobs.withTrimmedTop (34);
        for (int i = 0; i < 4; ++i) hk[i]->setBounds (kr.removeFromLeft (kw).reduced (3, 0).withHeight (std::min (124, kr.getHeight())));
        sourceBarArea = c.removeFromTop (30);
        layoutSourceBar (sourceBarArea);
        c.removeFromTop (4);
        hGraph.setBounds (c);
    }
    // ---- performance row
    {
        auto r = perfRow.reduced (12, 8);
        auto wheels = r.removeFromLeft (84);
        pitchWheel.setBounds (wheels.removeFromLeft (42));
        modWheel.setBounds (wheels);
        r.removeFromLeft (8);
        auto btns = r.removeFromRight (std::min (300, r.getWidth() / 3));
        r.removeFromRight (10);
        keyboard.setBounds (r);
        const int whites = juce::jlimit (14, 36, r.getWidth() / 28);
        int low = 36;
        if (whites < 22) low = 48;
        keyboard.setRange (low, whites);
        auto icons = btns.removeFromRight (36);
        panic.setBounds (icons.removeFromTop (icons.getHeight() / 2).reduced (2));
        tailKill.setBounds (icons.reduced (2));
        btns.removeFromRight (6);
        auto row1 = btns.removeFromTop (btns.getHeight() / 2);
        latch.setBounds (row1.removeFromLeft (row1.getWidth() / 2).reduced (3, 4));
        freeze.setBounds (row1.reduced (3, 4));
        wetOnly.setBounds (btns.removeFromLeft (btns.getWidth() / 2).reduced (3, 4));
        advancedBtn.setBounds (btns.reduced (3, 4));
    }
    return y;
}

void MainView::layoutSourceBar (juce::Rectangle<int> r)
{
    const int src = (int) proc.value (NoteSource);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &policy, &chRoot, &chQuality, &chSpread, &intRef, &intMode, &intKey, &intScale, &arpMode, &arpRate,
                                &chOct, &chInv, &intRoot, &intCount, &arpOct, &storeArm, &arpSync, &arpGate })
        c->setVisible (false);
    for (auto& s : snap) s.setVisible (false);
    auto place = [&r] (juce::Component& c, int w) { c.setVisible (true); c.setBounds (r.removeFromLeft (std::min (w, std::max (0, r.getWidth()))).reduced (0, 2)); r.removeFromLeft (6); };
    switch (src)
    {
        case 0: place (policy, 300); midiLedArea = r.removeFromLeft (200); break;
        case 1:
            place (chRoot, 64); place (chQuality, 92); place (chOct, 86); place (chInv, 80); place (chSpread, 80);
            r.removeFromLeft (4);
            for (auto& s : snap) place (s, 26);
            place (storeArm, 58);
            break;
        case 2: place (intRoot, 110); place (intRef, 108); place (intMode, 100); place (intKey, 56); place (intScale, 130); place (intCount, 96); break;
        default: place (arpMode, 100); place (arpRate, 70); place (arpSync, 56); place (arpOct, 86); place (arpGate, 110); break;
    }
}

void MainView::paintCardHeader (juce::Graphics& g, juce::Rectangle<int> card, const juce::String& title, const juce::String& subtitle)
{
    auto top = card.reduced (14, 10).removeFromTop (34).withTrimmedLeft (26);
    g.setColour (theme::text);
    g.setFont (theme::font (24.0f, 3));
    g.drawText (title, top, juce::Justification::centredLeft);
    const int tw = (int) juce::GlyphArrangement::getStringWidth (theme::font (24.0f, 3), title);
    if (subtitle.isNotEmpty() && top.getWidth() > tw + 60)
    {
        g.setColour (theme::textMuted);
        g.setFont (theme::spaced (11.0f, 0.32f, 0));
        g.drawText (subtitle, top.withTrimmedLeft (tw + 14).withTrimmedTop (4), juce::Justification::centredLeft);
    }
}

void MainView::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (theme::shellTop, 0, 0, theme::shellBottom, 0, (float) getHeight(), false));
    g.fillAll();
    // header
    {
        auto h = header.reduced (8, 4);
        g.setColour (theme::text);
        const float th = getWidth() < 1150 ? 25.0f : 34.0f;
        g.setFont (theme::font (th, 3));
        const int w1 = (int) juce::GlyphArrangement::getStringWidth (theme::font (th, 3), "PLAYABLE ");
        auto t = h.removeFromTop (44);
        g.drawText ("PLAYABLE", t, juce::Justification::bottomLeft);
        g.setFont (theme::font (th, 0));
        g.drawText ("AMBIENCE", t.withTrimmedLeft (w1), juce::Justification::bottomLeft);
        g.setColour (theme::textMuted);
        g.setFont (theme::spaced (11.5f, 0.55f, 0));
        g.drawText ("VOCODED DELAY & REVERB", h.removeFromTop (20), juce::Justification::centredLeft);
        // preset box
        auto pb = presetName.getBounds().getUnion (prevPreset.getBounds()).getUnion (nextPreset.getBounds()).toFloat();
        paintCard (g, pb, 8.0f);
        g.setColour (theme::border);
        g.drawVerticalLine (prevPreset.getRight(), pb.getY() + 4, pb.getBottom() - 4);
        g.drawVerticalLine (nextPreset.getX(), pb.getY() + 4, pb.getBottom() - 4);
        // separators
        g.setColour (theme::border);
        g.drawVerticalLine (abSeg.getX() - 6, (float) header.getY() + 14, (float) header.getBottom() - 14);
        g.drawVerticalLine (inMeter.getX() - 6, (float) header.getY() + 14, (float) header.getBottom() - 14);
        // dry/wet indicator under preset box
        g.setColour (proc.value (WetOnly) > 0.5f ? theme::accent : theme::textMuted);
        g.setFont (theme::spaced (10.5f, 0.18f, 2));
        g.drawText (dryWetText, presetName.getBounds().withY (pb.toNearestInt().getBottom() + 1).withHeight (14).expanded (60, 0), juce::Justification::centred);
    }
    // cards
    paintCard (g, dCard.toFloat());
    paintCard (g, rCard.toFloat());
    paintCardHeader (g, dCard, "DELAY", dCollapsed && ! stacked ? juce::String() : "ECHO & INTERVAL");
    paintCardHeader (g, rCard, "REVERB", rCollapsed && ! stacked ? juce::String() : "SPACE & SUSTAIN");
    auto collapsedNote = [&g] (juce::Rectangle<int> card, const juce::String& what) {
        g.setColour (theme::textMuted);
        g.setFont (theme::font (13.0f, 1));
        g.drawFittedText (what + " OFF\nprocessing disabled", card.reduced (14, 10).withTrimmedTop (140).withHeight (60), juce::Justification::centredTop, 3);
    };
    if (dCollapsed && ! stacked) collapsedNote (dCard, "DELAY");
    if (rCollapsed && ! stacked) collapsedNote (rCard, "REVERB");
    // routing strip
    paintCard (g, routeStrip.toFloat(), 10.0f);
    g.setColour (theme::text);
    g.setFont (theme::spaced (12.0f, 0.12f, 2));
    g.drawText ("ROUTING", routing.getBounds().withX (routing.getX() - 80).withWidth (74), juce::Justification::centredRight);
    g.drawText ("PLACEMENT", placement.getBounds().withX (placement.getX() - 100).withWidth (92), juce::Justification::centredRight);
    g.setColour (theme::border);
    g.drawVerticalLine (routeStrip.getCentreX(), (float) routeStrip.getY() + 8, (float) routeStrip.getBottom() - 8);
    g.setColour (theme::textMuted);
    g.setFont (theme::font (11.5f));
    {
        auto st = routeStrip.reduced (12, 0);
        auto area = st.withTrimmedLeft (placement.getRight() - st.getX() + 12);
        g.drawFittedText (statusText, area, juce::Justification::centredRight, 2, 0.8f);
    }
    // harmony
    paintCard (g, hCard.toFloat());
    paintCardHeader (g, hCard, "HARMONY", "PLAY THE WET SIGNAL");
    if ((int) proc.value (NoteSource) == 0 && ! midiLedArea.isEmpty())
    {
        auto led = midiLedArea.withSizeKeepingCentre (midiLedArea.getWidth(), 20).withTrimmedLeft (8);
        auto dot = led.removeFromLeft (14).toFloat().withSizeKeepingCentre (9, 9);
        g.setColour (theme::accent.withAlpha (0.2f + 0.8f * midiLed));
        g.fillEllipse (dot);
        g.setColour (theme::textMuted);
        g.setFont (theme::font (11.5f));
        g.drawText (proc.isCompanion() ? "MIDI n/a (Audio AU): use on-screen keys / CHORD" : sourceInfo, led.withTrimmedLeft (6), juce::Justification::centredLeft);
    }
    else if ((int) proc.value (NoteSource) == 2 && ! sourceBarArea.isEmpty())
    {
        g.setColour (theme::textMuted);
        g.setFont (theme::font (11.5f));
        g.drawText (sourceInfo, sourceBarArea.withTrimmedLeft (intCount.getRight() - sourceBarArea.getX() + 10), juce::Justification::centredLeft);
    }
    // performance row
    paintCard (g, perfRow.toFloat());
}

void MainView::update()
{
    auto& t = proc.telemetry();
    inMeter.setLevels (t.inPeakL.exchange (0.0f), t.inPeakR.exchange (0.0f));
    outMeter.setLevels (t.outPeakL.exchange (0.0f), t.outPeakR.exchange (0.0f));
    const int mc = t.midiCounter.load();
    if (mc != lastMidiCount) { midiLed = 1.0f; lastMidiCount = mc; } else midiLed *= 0.85f;
    // layout changes
    const int key = (proc.value (DelayEnable) > 0.5f ? 1 : 0) + (proc.value (ReverbEnable) > 0.5f ? 2 : 0) + (int) proc.value (NoteSource) * 4
                    + (int) proc.value (DelayMode) * 32 + (int) proc.value (ReverbMode) * 64 + (proc.value (BbdSync) > 0.5f ? 128 : 0) + (proc.value (IvSync) > 0.5f ? 256 : 0);
    if (key != lastLayoutKey) { lastLayoutKey = key; layout (getWidth()); repaint(); }
    dGraph.setOverlayText (proc.value (DelayEnable) > 0.5f ? juce::String() : "DELAY OFF - click the dot to enable");
    rGraph.setOverlayText (proc.value (ReverbEnable) > 0.5f ? juce::String() : "REVERB OFF - click the dot to enable");
    if (dGraph.isVisible()) dGraph.update();
    if (rGraph.isVisible()) rGraph.update();
    hGraph.update();
    keyboard.update();
    for (auto& k : dk) k->refreshValue();
    for (auto& k : rk) k->refreshValue();
    for (auto& k : hk) k->refreshValue();
    output.refreshValue();
    dDot.refresh(); rDot.refresh(); mDot.repaint();
    // harmony controls greyed when method is Off
    const bool hOn = proc.value (HarmMethod) > 0.5f;
    hk[0]->setEnabled (hOn); hk[1]->setEnabled (hOn); hk[2]->setEnabled (hOn);
    placement.setEnabled (hOn);
    // status line
    juce::String s;
    const int timing = (int) proc.value (Timing);
    s << (timing == 1 ? "STUDIO " + juce::String (proc.reportedLatency()) + " smp" : "LIVE  wet +" + juce::String (t.wetLatency.load()) + " smp");
    if (proc.latencyChangePending()) s << "  |  stop transport to apply timing/quality";
    if (t.protectionActive.exchange (false)) s << "  |  WET LIMITER";
    if (t.clearing.load() == 2) s << "  |  clearing tail";
    if (t.nonfiniteCount.load() > 0) s << "  |  wet reset x" << t.nonfiniteCount.load();
    if (proc.isCompanion()) s << "  |  Audio AU: no MIDI input";
    if (s != statusText) { statusText = s; repaint (routeStrip); }
    const juce::String dw = proc.value (WetOnly) > 0.5f ? "WET ONLY" : "DRY + WET   DRY " + juce::String (paramValueToText (DryLevel, proc.value (DryLevel)));
    if (dw != dryWetText) { dryWetText = dw; repaint (header); }
    // source info
    juce::String si;
    const int src = (int) proc.value (NoteSource);
    if (src == 0) si = "MIDI in (" + juce::String (mc) + ")" + (t.holdingLast.load() ? "   holding last chord" : "");
    else if (src == 2)
    {
        si = "offsets";
        for (int k = 0; k < (int) proc.value (IntCount); ++k) { const int o = (int) proc.value (Int1 + k); si << " " << (o >= 0 ? "+" : "") << o; }
        si << ((int) proc.value (IntMode) == 0 ? " st" : " steps");
    }
    if (si != sourceInfo) { sourceInfo = si; repaint (hCard); }
    else if (midiLed > 0.05f) repaint (midiLedArea);
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
    viewport.setViewedComponent (view.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);
    addAndMakeVisible (viewport);
    if (p.isStandalone() && p.source() != nullptr)
    {
        standalone = std::make_unique<StandalonePanel> (p);
        standalone->onSaveExperiment = [this] { saveExperiment(); };
        addAndMakeVisible (*standalone);
    }
    proc.presets().onChange = [this] { juce::Component::SafePointer<PluginEditor> sp (this); juce::MessageManager::callAsync ([sp] { if (sp) sp->view->refreshPresetName(); }); };
    setResizable (true, true);
    const int extra = standalone ? 82 : 0;
    setResizeLimits (950, 680, 2400, 1700);
    setSize (1280, 836 + extra);
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

void PluginEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (theme::shellTop, 0, 0, theme::shellBottom, 0, (float) getHeight(), false));
    g.fillAll();
}

void PluginEditor::resized()
{
    auto r = getLocalBounds();
    if (standalone) standalone->setBounds (r.removeFromTop (82).reduced (14, 6).withTrimmedBottom (-4));
    viewport.setBounds (r);
    const int w = r.getWidth() - (view->getHeight() > r.getHeight() ? viewport.getScrollBarThickness() : 0);
    const int h = view->layout (w);
    view->setSize (w, h);
    if (h > r.getHeight() && w == r.getWidth()) { const int w2 = r.getWidth() - viewport.getScrollBarThickness(); view->setSize (w2, view->layout (w2)); }
    if (advanced) advanced->setBounds (getLocalBounds());
}

void PluginEditor::timerCallback()
{
    view->update();
    ++frame;
}

void PluginEditor::refreshForSnapshot()
{
    resized();
    for (int i = 0; i < 3; ++i) view->update();
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
