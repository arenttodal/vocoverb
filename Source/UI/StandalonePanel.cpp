#include "StandalonePanel.h"

#include "Presets/PresetManager.h"
#include "Standalone/StandaloneHost.h"

namespace pa
{
namespace
{
constexpr int kDemoBase = 1, kFileId = 50, kExpBase = 100;
}

StandalonePanel::StandalonePanel (PluginProcessor& p) : proc (p), src (*p.source())
{
    sourceBox.addSectionHeading ("Demo (synthetic)");
    for (int i = 0; i < (int) Fixture::Count; ++i) sourceBox.addItem (fixtureInfo ((Fixture) i).name, kDemoBase + i);
    sourceBox.addSectionHeading ("Experiments (repeatable sequences)");
    const auto& ex = builtInExperiments();
    for (int i = 0; i < (int) ex.size(); ++i) sourceBox.addItem (ex[(size_t) i].name, kExpBase + i);
    sourceBox.addSectionHeading ("File");
    sourceBox.addItem ("Loaded file", kFileId);
    sourceBox.setItemEnabled (kFileId, false);
    sourceBox.setSelectedId (kDemoBase + (int) Fixture::HarmonicTone, juce::dontSendNotification);
    sourceBox.onChange = [this] { chooseSource (sourceBox.getSelectedId()); };
    sourceBox.setTooltip ("Source to audition: synthetic demos, repeatable experiments (with MIDI sequences) or a loaded file");
    sourceBox.setTitle ("Source");
    addAndMakeVisible (sourceBox);

    loadFile.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Load audio (WAV / AIFF / FLAC, up to 2 minutes)", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) loadAudioFile (fc.getResult()); });
    };
    loadFile.setTooltip ("Load a WAV, AIFF or FLAC file (you can also drag and drop onto the window)");
    play.onClick = [this] { if (src.isPlaying()) src.pause(); else src.play(); };
    stop.onClick = [this] { src.stop(); };
    loop.setClickingTogglesState (true);
    loop.setToggleState (src.isLooping(), juce::dontSendNotification);
    loop.onClick = [this] { src.setLoop (loop.getToggleState()); };
    keepTail.setClickingTogglesState (true);
    keepTail.setToggleState (src.keepTailOnStop(), juce::dontSendNotification);
    keepTail.onClick = [this] { src.setKeepTailOnStop (keepTail.getToggleState()); };
    keepTail.setTooltip ("When on, Stop lets the ambience ring; when off, Stop also clears the wet tail");
    liveInput.setClickingTogglesState (true);
    liveInput.setTooltip ("Process the selected audio input device. Use headphones to avoid feedback. macOS asks for microphone access the first time.");
    liveInput.onClick = [this] {
        const bool on = liveInput.getToggleState();
        if (auto* h = StandaloneHost::instance())
        {
            if (on)
                juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Live Input",
                    "Live input will be processed and monitored through your output. Use headphones to avoid feedback.\n\nmacOS may ask for microphone permission now.",
                    "Enable", "Cancel", nullptr, juce::ModalCallbackFunction::create ([this, h] (int r) {
                        if (r == 1) h->setLiveInput (true);
                        else liveInput.setToggleState (false, juce::dontSendNotification);
                    }));
            else h->setLiveInput (false);
        }
    };
    recMidi.setClickingTogglesState (true);
    recMidi.onClick = [this] { src.setRecording (recMidi.getToggleState()); };
    settings.onClick = [] { if (auto* h = StandaloneHost::instance()) h->showAudioMidiSettings(); };
    settings.setTooltip ("Audio device, sample rate, buffer size, channels and MIDI inputs");
    exportBtn.onClick = [this] { exportAudition(); };
    exportBtn.setTooltip ("Offline-render the current source (+ tail) with the current settings and MIDI to a 24-bit WAV");
    saveExp.onClick = [this] { if (onSaveExperiment) onSaveExperiment(); };
    saveExp.setTooltip ("Write the preset, settings, recorded MIDI and diagnostics to a folder for feedback");
    level.setSliderStyle (juce::Slider::LinearHorizontal);
    level.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    level.setRange (-40.0, 6.0, 0.1);
    level.setValue (-6.0);
    level.setDoubleClickReturnValue (true, -6.0);
    level.setTooltip ("Source level (separate from dry/wet mix)");
    level.setTitle ("Source level");
    level.onValueChange = [this] { src.setLevelDb ((float) level.getValue()); };
    src.setLevelDb (-6.0f);
    seek.src = &src;
    status.setFont (theme::font (12.0f));
    status.setColour (juce::Label::textColourId, theme::textMuted);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &loadFile, &settings, &keepTail, &liveInput, &exportBtn, &saveExp, &play, &stop, &loop, &recMidi,
                                &level, &seek, &status })
        addAndMakeVisible (c);
    exporter.onFinished = [this] (bool, juce::String msg) { status.setText (msg, juce::dontSendNotification); exportBtn.setButtonText ("Export WAV"); };
    src.selectDemo ((int) Fixture::HarmonicTone);
    startTimerHz (15);
}

StandalonePanel::~StandalonePanel() { stopTimer(); }

void StandalonePanel::chooseSource (int id)
{
    if (id >= kExpBase) { src.selectExperiment (id - kExpBase); src.setLoop (false); loop.setToggleState (false, juce::dontSendNotification); }
    else if (id >= kDemoBase && id < kDemoBase + (int) Fixture::Count) src.selectDemo (id - kDemoBase);
}

void StandalonePanel::loadAudioFile (const juce::File& f)
{
    juce::String err;
    if (! src.loadFile (f, err)) { status.setText (err, juce::dontSendNotification); return; }
    sourceBox.setItemEnabled (kFileId, true);
    sourceBox.changeItemText (kFileId, f.getFileName());
    sourceBox.setSelectedId (kFileId, juce::dontSendNotification);
}

void StandalonePanel::exportAudition()
{
    if (exporter.isBusy()) { exporter.cancel(); return; }
    juce::PopupMenu m;
    for (double t : { 5.0, 10.0, 20.0, 30.0 }) m.addItem (juce::String ((int) t) + " s tail", true, tailSeconds == t, [this, t] { tailSeconds = t; });
    m.addSeparator();
    m.addItem ("Export...", [this] {
        auto data = src.currentData();
        if (data == nullptr) { status.setText ("No source to export.", juce::dontSendNotification); return; }
        chooser = std::make_unique<juce::FileChooser> ("Export audition WAV",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("PlayableAmbience-" + juce::File::createLegalFileName (proc.presets().currentName()) + ".wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, [this, data] (const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            AuditionExporter::Job job;
            job.params = proc.currentParams();
            job.source.makeCopyOf (data->audio);
            job.sampleRate = data->sampleRate;
            job.tailSeconds = tailSeconds;
            job.output = f.withFileExtension ("wav");
            // explicit MIDI: experiment sequence, else recorded MIDI, else none (stored chord / policy applies)
            if (! data->events.empty()) job.events = data->events;
            else if (src.recordedCount() > 0) job.events = src.recordedEvents();
            juce::String err;
            if (exporter.start (std::move (job), err)) exportBtn.setButtonText ("Cancel");
            else status.setText (err, juce::dontSendNotification);
        });
    });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (exportBtn));
}

void StandalonePanel::timerCallback()
{
    play.setToggleState (src.isPlaying(), juce::dontSendNotification);
    if (auto* h = StandaloneHost::instance()) liveInput.setToggleState (h->liveInputActive(), juce::dontSendNotification);
    juce::String s;
    if (exporter.isBusy()) s = "Exporting... " + juce::String ((int) (exporter.progress() * 100.0f)) + " %";
    else if (src.loadingText().isNotEmpty()) s = src.loadingText();
    else
    {
        s = src.statusText();
        if (auto* h = StandaloneHost::instance()) s << "   |   " << h->deviceSummary();
        if (src.isRecording()) s << "   |   REC MIDI " << src.recordedCount();
        if (! (StandaloneHost::instance() && StandaloneHost::instance()->liveInputActive())) s << "   |   live input off";
    }
    status.setText (s, juce::dontSendNotification);
    seek.repaint();
}

void StandalonePanel::paint (juce::Graphics& g)
{
    paintCard (g, getLocalBounds().toFloat().reduced (0.5f), 10.0f);
    g.setColour (theme::text);
    g.setFont (theme::spaced (11.0f, 0.2f, 2));
    g.drawText ("SOURCE", getLocalBounds().reduced (12, 0).removeFromLeft (60), juce::Justification::centredLeft);
}

void StandalonePanel::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    auto row1 = r.removeFromTop (28);
    row1.removeFromLeft (60);
    sourceBox.setBounds (row1.removeFromLeft (240).reduced (0, 1));
    row1.removeFromLeft (6);
    loadFile.setBounds (row1.removeFromLeft (94));
    row1.removeFromLeft (10);
    play.setBounds (row1.removeFromLeft (30));
    stop.setBounds (row1.removeFromLeft (30));
    loop.setBounds (row1.removeFromLeft (30));
    row1.removeFromLeft (8);
    keepTail.setBounds (row1.removeFromRight (84));
    row1.removeFromRight (8);
    level.setBounds (row1.removeFromRight (110));
    row1.removeFromRight (8);
    seek.setBounds (row1.reduced (0, 4));
    r.removeFromTop (6);
    auto row2 = r.removeFromTop (26);
    settings.setBounds (row2.removeFromRight (104));
    row2.removeFromRight (6);
    saveExp.setBounds (row2.removeFromRight (122));
    row2.removeFromRight (6);
    exportBtn.setBounds (row2.removeFromRight (96));
    row2.removeFromRight (10);
    recMidi.setBounds (row2.removeFromRight (28));
    row2.removeFromRight (4);
    liveInput.setBounds (row2.removeFromRight (92));
    row2.removeFromRight (10);
    status.setBounds (row2.withTrimmedLeft (58));
}

void StandalonePanel::SeekBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::display);
    g.fillRoundedRectangle (r, 4.0f);
    if (src == nullptr) return;
    const double dur = src->durationSeconds();
    if (dur <= 0) return;
    const double pos = juce::jlimit (0.0, dur, src->positionSeconds());
    auto data = src->currentData();
    if (data)
        for (double m : data->markers)
        {
            const float x = r.getX() + r.getWidth() * (float) (m / dur);
            g.setColour (theme::amber.withAlpha (0.7f));
            g.fillRect (x - 0.5f, r.getY() + 2.0f, 1.5f, r.getHeight() - 4.0f);
        }
    g.setColour (theme::accent);
    g.fillRoundedRectangle (r.withWidth (r.getWidth() * (float) (pos / dur)).reduced (0, 6.0f), 2.0f);
    g.setColour (theme::ivoryTrace);
    g.setFont (theme::font (11.0f));
    g.drawText (juce::String (pos, 1) + " / " + juce::String (dur, 1) + " s", r.reduced (6.0f, 0), juce::Justification::centredRight);
}

void StandalonePanel::SeekBar::mouseDrag (const juce::MouseEvent& e)
{
    if (src) src->seek (src->durationSeconds() * juce::jlimit (0.0, 1.0, (double) e.x / getWidth()));
}
} // namespace pa
