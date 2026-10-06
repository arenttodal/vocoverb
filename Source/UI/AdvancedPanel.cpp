#include "AdvancedPanel.h"

#include "Presets/PresetManager.h"

namespace pa
{
struct AdvancedPanel::Row : public juce::Component
{
    enum class Type { Param, Section, Buttons, Info };
    Type type = Type::Param;
    int index = -1;
    juce::String title, help, why;
    std::function<bool()> enabledFn;
    std::function<juce::String()> infoFn;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::Label> valueLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
    juce::OwnedArray<juce::TextButton> buttons;
    PluginProcessor* proc = nullptr;
    bool lastEnabled = true;

    int preferredHeight() const
    {
        switch (type)
        {
            case Type::Section: return help.isEmpty() ? 34 : 52;
            case Type::Buttons: return 40;
            case Type::Info: return 44;
            default: return 36;
        }
    }

    void refresh()
    {
        const bool en = enabledFn ? enabledFn() : true;
        if (en != lastEnabled) { lastEnabled = en; setEnabled (en); repaint(); }
        if (valueLabel && proc && index >= 0)
            if (auto* p = proc->param (index)) valueLabel->setText (p->getCurrentValueAsText(), juce::dontSendNotification);
        if (type == Type::Info) repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 3);
        if (type == Type::Param)
        {
            r.removeFromLeft (190);
            auto ctl = r.removeFromLeft (250);
            auto val = r.removeFromLeft (92);
            if (slider) slider->setBounds (ctl.reduced (2, 4));
            if (toggle) toggle->setBounds (ctl.reduced (2, 2));
            if (combo) combo->setBounds (ctl.reduced (2, 4));
            if (valueLabel) valueLabel->setBounds (val.reduced (4, 5));
        }
        else if (type == Type::Buttons)
        {
            for (auto* b : buttons)
            {
                const int w = std::max (90, (int) juce::GlyphArrangement::getStringWidth (theme::font (12.5f, 1), b->getButtonText()) + 26);
                b->setBounds (r.removeFromLeft (w).reduced (2, 3));
                r.removeFromLeft (4);
            }
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (6, 2);
        const bool en = isEnabled();
        if (type == Type::Section)
        {
            g.setColour (theme::border);
            g.drawHorizontalLine (r.getY() + 4, (float) r.getX(), (float) r.getRight());
            g.setColour (en ? theme::text : theme::textDisabled);
            g.setFont (theme::spaced (13.0f, 0.2f, 3));
            g.drawText (title.toUpperCase(), r.withHeight (26).withTrimmedTop (6), juce::Justification::centredLeft);
            g.setFont (theme::font (12.5f));
            g.setColour (theme::textMuted);
            juce::String t = help;
            if (! en && why.isNotEmpty()) t = why + (help.isNotEmpty() ? "  " + help : juce::String());
            g.drawFittedText (t, r.withTrimmedTop (28), juce::Justification::topLeft, 2);
            return;
        }
        if (type == Type::Info)
        {
            g.setColour (theme::valueBox);
            g.fillRoundedRectangle (r.toFloat(), 5.0f);
            g.setColour (theme::text);
            g.setFont (theme::font (12.5f));
            g.drawFittedText (infoFn ? infoFn() : juce::String(), r.reduced (10, 4), juce::Justification::centredLeft, 3);
            return;
        }
        if (type != Type::Param) return;
        g.setColour (en ? theme::text : theme::textDisabled);
        g.setFont (theme::font (13.5f, 1));
        g.drawFittedText (title, r.removeFromLeft (186), juce::Justification::centredLeft, 1);
        r.removeFromLeft (4 + 250 + 92);
        g.setColour (en ? theme::textMuted : theme::textDisabled);
        g.setFont (theme::font (12.0f));
        g.drawFittedText (en || why.isEmpty() ? help : why, r.reduced (8, 0), juce::Justification::centredLeft, 2, 0.9f);
    }
};

struct AdvancedPanel::Content : public juce::Component
{
    juce::OwnedArray<Row> rows;
    void layout (int width)
    {
        int y = 4;
        for (auto* r : rows) { r->setBounds (0, y, width, r->preferredHeight()); y += r->preferredHeight(); }
        setSize (width, y + 12);
    }
};

AdvancedPanel::AdvancedPanel (PluginProcessor& p, std::function<void()> onClose)
    : proc (p), closeFn (std::move (onClose)), tabs (nullptr, -1, tabNames)
{
    tabs.setFontHeight (12.0f);
    tabs.onSelect = [this] (int i) { build (i); proc.uiAdvancedTab = tabNames[i]; };
    addAndMakeVisible (tabs);
    closeButton.onClick = [this] { if (closeFn) closeFn(); };
    addAndMakeVisible (closeButton);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);
    addAndMakeVisible (viewport);
    diagText.setMultiLine (true);
    diagText.setReadOnly (true);
    diagText.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::plain));
    diagText.setColour (juce::TextEditor::backgroundColourId, theme::valueBox);
    setWantsKeyboardFocus (true);
    setTitle ("Advanced settings");
    showTab (proc.uiAdvancedTab);
    startTimerHz (10);
}

AdvancedPanel::~AdvancedPanel() { stopTimer(); }

void AdvancedPanel::showTab (const juce::String& name)
{
    const int i = juce::jmax (0, tabNames.indexOf (name));
    tabs.setSelected (i, false);
    build (i);
}

bool AdvancedPanel::keyPressed (const juce::KeyPress& k)
{
    if (k.isKeyCode (juce::KeyPress::escapeKey)) { if (closeFn) closeFn(); return true; }
    return false;
}

void AdvancedPanel::addSection (const juce::String& title, const juce::String& note, std::function<bool()> enabled, const juce::String& why)
{
    auto* r = content->rows.add (new Row());
    r->type = Row::Type::Section; r->title = title; r->help = note; r->enabledFn = enabled; r->why = why;
    sectionEnabled = enabled; sectionWhy = why;
    content->addAndMakeVisible (r);
}

void AdvancedPanel::addParamRow (int index, std::function<bool()> enabled, const juce::String& why)
{
    auto* r = content->rows.add (new Row());
    r->type = Row::Type::Param; r->index = index; r->proc = &proc;
    const auto& info = paramInfo (index);
    r->title = info.name; r->help = info.help;
    auto secEn = sectionEnabled;
    if (enabled && secEn) r->enabledFn = [enabled, secEn] { return enabled() && secEn(); };
    else r->enabledFn = enabled ? enabled : secEn;
    r->why = why.isNotEmpty() ? why : sectionWhy;
    switch (info.kind)
    {
        case Kind::Bool:
            r->toggle = std::make_unique<juce::ToggleButton> ("");
            r->ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, info.id, *r->toggle);
            r->toggle->setTitle (info.name);
            r->addAndMakeVisible (*r->toggle);
            break;
        case Kind::Choice:
        {
            r->combo = std::make_unique<juce::ComboBox>();
            juce::StringArray ch; for (auto& c : paramChoices (index)) ch.add (c);
            r->combo->addItemList (ch, 1);
            r->ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, info.id, *r->combo);
            r->combo->setTitle (info.name);
            r->addAndMakeVisible (*r->combo);
            break;
        }
        default:
        {
            r->slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            r->sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, info.id, *r->slider);
            if (auto* p = proc.param (index)) r->slider->setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            r->slider->setTitle (info.name);
            r->slider->setDescription (info.help);
            r->slider->setVelocityModeParameters (0.35, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
            r->addAndMakeVisible (*r->slider);
            r->valueLabel = std::make_unique<juce::Label>();
            r->valueLabel->setEditable (false, true, false);
            r->valueLabel->setJustificationType (juce::Justification::centredRight);
            r->valueLabel->setFont (theme::font (13.0f));
            r->valueLabel->setColour (juce::Label::backgroundColourId, theme::valueBox);
            r->valueLabel->setColour (juce::Label::outlineColourId, theme::border);
            auto* rp = r;
            r->valueLabel->onTextChange = [this, rp] {
                if (auto* p = proc.param (rp->index)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getValueForText (rp->valueLabel->getText())); p->endChangeGesture(); }
            };
            r->addAndMakeVisible (*r->valueLabel);
            break;
        }
    }
    r->refresh();
    content->addAndMakeVisible (r);
}

void AdvancedPanel::addButtons (std::vector<std::pair<juce::String, std::function<void()>>> buttons)
{
    auto* r = content->rows.add (new Row());
    r->type = Row::Type::Buttons;
    for (auto& [text, fn] : buttons)
    {
        auto* b = r->buttons.add (new juce::TextButton (text));
        b->onClick = fn;
        r->addAndMakeVisible (b);
    }
    content->addAndMakeVisible (r);
}

void AdvancedPanel::addInfo (std::function<juce::String()> textFn)
{
    auto* r = content->rows.add (new Row());
    r->type = Row::Type::Info; r->infoFn = std::move (textFn);
    content->addAndMakeVisible (r);
}

void AdvancedPanel::build (int tab)
{
    viewport.setViewedComponent (nullptr, false);
    content = std::make_unique<Content>();
    sectionEnabled = nullptr; sectionWhy = {};
    auto& P = proc;
    auto range = [this] (int a, int b) { for (int i = a; i <= b; ++i) addParamRow (i); };
    diagText.setVisible (false);

    switch (tab)
    {
        case 0:
            addSection ("Harmony", "Classic filter-bank vocoder shared by both engines. Notes come from the Note Source (MIDI tab).");
            addParamRow (HarmEnable); addParamRow (Placement);
            addParamRow (ApplyHarmonyTo, [&P] { return (int) P.value (Routing) == 0; }, "Only used with Parallel routing (series routes harmonise the combined wet once).");
            range (Depth, NoteRelease); addParamRow (Polyphony); addParamRow (NoNotePolicy); addParamRow (Latch);
            addSection ("Classic vocoder", "Log-spaced bands; rectified envelopes of the wet signal are imposed on the MIDI carrier. This build uses Classic only: "
                        "sessions saved with FFT, Resonator or Shift load as Classic, and a saved Off loads as Harmony Enable off.",
                        [&P] { return effectiveHarmonyMethod (P.currentParams()) != 0; }, "Inactive: Harmony is off.");
            range (ClBands, ClNoise);
            sectionEnabled = nullptr;
            break;
        case 1:
            addSection ("Delay engine", "Two MIDI destinations exist: Harmony (absolute carrier notes) and the Interval Pitch Driver (relative tap transposition).");
            addParamRow (DelayEnable); addParamRow (DelayMode); addParamRow (ClearTailOnChange);
            addSection ("BBD", {}, [&P] { return P.value (DelayMode) < 0.5f; }, "Inactive: BBD mode not selected (values are kept).");
            range (BbdTime, BbdLevel);
            addSection ("Interval", "Stable: tempo independent of pitch. Clock: rate couples pitch and fragment duration. Reverse adds acquisition delay.",
                        [&P] { return P.value (DelayMode) > 0.5f; }, "Inactive: Interval mode not selected (values are kept).");
            range (IvTime, IvLevel);
            sectionEnabled = nullptr;
            addSection ("Freeze", "Freeze never creates sound on its own. Delay freeze loops a crossfaded capture of the recent delay output.");
            addParamRow (FreezeTarget); addParamRow (FreezeOverdub);
            break;
        case 2:
            addSection ("Reverb engine", {});
            addParamRow (ReverbEnable); addParamRow (ReverbMode); addParamRow (Width);
            addSection ("Plate", "Dattorro-topology plate tank (original implementation) with stereo input and low-band decay.",
                        [&P] { return P.value (ReverbMode) < 0.5f; }, "Inactive: Plate not selected (values are kept).");
            range (PlDecay, PlLevel);
            addSection ("Wash", "Modulated 8/16-line FDN with Hadamard feedback, input diffusion and bloom stage. Freeze = near-lossless hold with energy guard.",
                        [&P] { return P.value (ReverbMode) > 0.5f; }, "Inactive: Wash not selected (values are kept).");
            range (WaDecay, WaLevel);
            break;
        case 3:
        {
            addSection ("MIDI & notes", P.isCompanion() ? "MIDI input is unavailable in the Audio AU: use Chord, Intervals (stored root), Arp over the stored chord, or the on-screen keys." : juce::String());
            addParamRow (NoteSource); addParamRow (MidiChannel); addParamRow (PitchBendRange); addParamRow (ModWheelTarget);
            addParamRow (ChordWindow); addParamRow (RefTuning); addParamRow (Tempo);
            addButtons ({ { "Panic (notes off)", [&P] { P.panic(); } }, { "Tail Kill (notes + wet tails)", [&P] { P.tailKill(); } } });
            addSection ("Stored chord", "Eight snapshots: click to recall; MIDI Program Change 0-7 also recalls them.");
            range (ChRoot, ChCustom6);
            std::vector<std::pair<juce::String, std::function<void()>>> rec, store;
            for (int s = 0; s < 8; ++s)
            {
                rec.push_back ({ "Recall " + juce::String (s + 1), [&P, s] { P.recallSnapshot (s); } });
                store.push_back ({ "Store " + juce::String (s + 1), [&P, s] { P.storeSnapshot (s); } });
            }
            addButtons (rec); addButtons (store);
            addSection ("Intervals", "Chromatic offsets are semitones; Scale offsets are diatonic steps in the chosen key (a scale third is not always +4).");
            range (IntRoot, Int6);
            addSection ("Arpeggiator", "Traverses held MIDI notes, or the stored chord when none are held. Uses host tempo/position when playing.");
            range (ArpMode, ArpVelVar);
            break;
        }
        case 4:
            addSection ("Mix", "DRY / WET blends dry against wet (50% = both at full level). Output (Wet Level) and Dry Level are the trims the header knob no longer shows. "
                               "Dry is touched only by Dry Level, DRY / WET and Studio alignment; everything else acts on the wet branch.");
            addParamRow (Mix);
            range (DryLevel, InputSource);
            addParamRow (Routing);
            addParamRow (SerialSend, [&P] { return (int) P.value (Routing) != 0; }, "Only used with series routing.");
            range (ClearTailOnChange, ClearTailOnChange);
            range (Width, DuckRelease);
            addSection ("Timing & quality", "Live: immediate dry, wet carries method latency. Studio: fixed latency reported to the host. In a host, changes wait for transport stop.");
            addParamRow (Timing); addParamRow (Quality);
            addInfo ([&P] {
                juce::String s = "Reported latency " + juce::String (P.reportedLatency()) + " samples. Wet processing delay "
                               + juce::String (P.telemetry().wetLatency.load()) + " samples. Studio latency at this quality: "
                               + juce::String (P.studioLatencyFor ((int) P.value (Quality))) + " samples.";
                if (P.latencyChangePending()) s << "  Pending: stop the host transport to apply.";
                return s;
            });
            addSection ("Freeze", {});
            addParamRow (Freeze); addParamRow (FreezeTarget); addParamRow (FreezeOverdub);
            addSection ("A/B comparison", "A/B stores the complete musical state including timing. Loudness match trims B's wet level only (bounded).");
            addParamRow (WetTrim);
            addButtons ({ { "Copy A > B", [&P] { P.presets().copyAB (0, 1); } }, { "Copy B > A", [&P] { P.presets().copyAB (1, 0); } },
                          { "Match B loudness to A", [&P] {
                                juce::String msg; P.presets().matchLoudness (msg);
                                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Loudness match", msg); } },
                          { P.clearTailOnAB ? "Clear tail on A/B: ON" : "Clear tail on A/B: OFF", [this, &P] {
                                P.clearTailOnAB = ! P.clearTailOnAB;
                                juce::Component::SafePointer<AdvancedPanel> sp (this);
                                juce::MessageManager::callAsync ([sp] { if (sp != nullptr) sp->build (4); }); } } });
            addInfo ([&P] {
                return "Measured wet loudness: A " + juce::String (P.presets().slotLoudnessDb (0), 1) + " dB, B " + juce::String (P.presets().slotLoudnessDb (1), 1)
                       + " dB (active slot: " + (P.presets().abSlot() == 0 ? "A" : "B") + ").";
            });
            break;
        default:
            addSection ("Diagnostics", "Local only: nothing is uploaded. Export writes JSON without audio or personal paths.");
            addButtons ({ { "Export Diagnostics...", [this] {
                              auto chooser = std::make_shared<juce::FileChooser> ("Export diagnostics", juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("PlayableAmbience-diagnostics.json"), "*.json");
                              chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser] (const juce::FileChooser& fc) {
                                  auto f = fc.getResult();
                                  if (f != juce::File()) f.replaceWithText (juce::JSON::toString (proc.diagnostics()));
                              }); } },
                          { "Save Experiment...", [this] { if (onSaveExperiment) onSaveExperiment(); } } });
            diagText.setVisible (true);
            refreshDiagnostics();
            break;
    }
    content->layout (std::max (600, viewport.getWidth() - viewport.getScrollBarThickness()));
    viewport.setViewedComponent (content.get(), false);
    if (tab == 5)
    {
        content->addAndMakeVisible (diagText);
        diagText.setBounds (6, content->getHeight(), content->getWidth() - 12, 380);
        content->setSize (content->getWidth(), content->getHeight() + 392);
    }
}

void AdvancedPanel::refreshDiagnostics()
{
    auto v = proc.diagnostics();
    juce::String s;
    if (auto* o = v.getDynamicObject())
        for (auto& kv : o->getProperties())
        {
            juce::String val = kv.value.isObject() ? juce::JSON::toString (kv.value, true) : (kv.value.isArray() ? juce::JSON::toString (kv.value, true) : kv.value.toString());
            s << kv.name.toString().paddedRight (' ', 30) << val << "\n";
        }
    diagText.setText (s, false);
}

void AdvancedPanel::timerCallback()
{
    if (content) for (auto* r : content->rows) r->refresh();
    if (diagText.isVisible()) refreshDiagnostics();
}

void AdvancedPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.22f));
    auto card = getLocalBounds().reduced (30, 24).toFloat();
    paintCard (g, card, 14.0f);
    g.setColour (theme::text);
    g.setFont (theme::font (22.0f, 3));
    g.drawText ("ADVANCED", card.reduced (22.0f, 14.0f).removeFromTop (30.0f), juce::Justification::centredLeft);
}

void AdvancedPanel::resized()
{
    auto card = getLocalBounds().reduced (30, 24);
    auto inner = card.reduced (20, 14);
    auto top = inner.removeFromTop (34);
    closeButton.setBounds (top.removeFromRight (34).reduced (3));
    top.removeFromLeft (150);
    tabs.setBounds (top.removeFromLeft (std::min (top.getWidth() - 10, 760)).reduced (0, 2));
    inner.removeFromTop (10);
    viewport.setBounds (inner);
    if (content) build (tabs.selected());
}
} // namespace pa
