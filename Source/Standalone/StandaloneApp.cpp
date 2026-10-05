// Custom standalone application (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP): audio/MIDI device handling without
// opening the microphone at launch, persistent device choice by name, and a headless --selftest mode.
#include <JuceHeader.h>

#if JucePlugin_Build_Standalone

#include <juce_audio_plugin_client/detail/juce_CreatePluginFilter.h>

#include "BuildInfo.h"
#include "Plugin/PluginProcessor.h"
#include "Presets/PresetManager.h"
#include "Standalone/AuditionExporter.h"
#include "Standalone/SourcePlayer.h"
#include "Standalone/StandaloneHost.h"
#include "UI/PluginEditor.h"

namespace pa
{
namespace
{
juce::PropertiesFile::Options propertyOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Playable Ambience";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.folderName = "Playable Ambience";
    return o;
}

// ------------------------------------------------------------------------------------------ self test
struct SelfTest
{
    juce::StringArray log;
    int failures = 0;
    void check (bool ok, const juce::String& what) { log.add ((ok ? "PASS  " : "FAIL  ") + what); if (! ok) ++failures; }

    static double rms (const juce::AudioBuffer<float>& b, int start, int len)
    {
        double s = 0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = start; i < start + len && i < b.getNumSamples(); ++i) s += (double) b.getSample (ch, i) * b.getSample (ch, i);
        return std::sqrt (s / std::max (1, len * b.getNumChannels()));
    }

    void run (const juce::File& screenshotDir)
    {
        log.add ("Playable Ambience " PA_BUILD_VERSION " (" PA_BUILD_GIT ") standalone self-test (no audio device, no microphone)");
        auto proc = juce::createPluginFilterOfType (juce::AudioProcessor::wrapperType_Standalone);
        auto* pp = dynamic_cast<PluginProcessor*> (proc.get());
        check (pp != nullptr, "processor constructed as standalone");
        if (pp == nullptr) return;
        proc->setPlayConfigDetails (2, 2, 48000.0, 256);
        proc->prepareToPlay (48000.0, 256);
        check ((int) pp->value (Timing) == 0, "standalone default timing is Live");

        // 1. demo source through the real processor path
        auto* src = pp->source();
        check (src != nullptr, "source player available in standalone");
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf (2, 256);
        juce::AudioBuffer<float> rec (2, 48000 * 6);
        if (src != nullptr)
        {
            src->selectDemo ((int) Fixture::HarmonicTone);
            src->play();
            for (int b = 0; b < 48000 * 6 / 256; ++b)
            {
                buf.clear();
                proc->processBlock (buf, midi);
                for (int ch = 0; ch < 2; ++ch) rec.copyFrom (ch, b * 256, buf, ch, 0, 256);
            }
            const double phrase = rms (rec, 9600, 96000), tail = rms (rec, 48000 * 4, 48000);
            log.add ("      demo phrase RMS " + juce::String (juce::Decibels::gainToDecibels (phrase), 1) + " dBFS, tail (after source) "
                     + juce::String (juce::Decibels::gainToDecibels (tail), 1) + " dBFS");
            check (phrase > 1.0e-3, "Play Demo produces audible output");
            check (tail > 1.0e-4, "ambience tail continues after the phrase");
            src->stop();
        }
        // 2. every factory preset loads and processes finite audio
        int okCount = 0;
        const auto& fp = factoryPresets();
        for (int i = 0; i < (int) fp.size(); ++i)
        {
            pp->presets().loadFactory (i);
            if (src) { src->selectDemo (i % 5); src->play(); }
            bool finite = true; double energy = 0;
            for (int b = 0; b < 48000 / 256; ++b)
            {
                buf.clear();
                proc->processBlock (buf, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int s = 0; s < 256; ++s) { const float v = buf.getSample (ch, s); finite &= std::isfinite (v); energy += (double) v * v; }
            }
            if (finite && energy > 0) ++okCount;
            else log.add ("      preset failed: " + juce::String (fp[(size_t) i].name));
        }
        check (okCount == (int) fp.size(), "all " + juce::String ((int) fp.size()) + " factory presets load and render finite, non-silent audio");
        if (src) src->stop();

        // 2b. double-precision host buffers and latency-matched bypass
        {
            pp->presets().loadFactory (5);
            juce::AudioBuffer<double> db (2, 256);
            bool finite = true; double e = 0;
            if (src) { src->selectDemo (0); src->play(); }
            for (int b = 0; b < 200; ++b)
            {
                db.clear();
                proc->processBlock (db, midi);
                for (int ch = 0; ch < 2; ++ch) for (int s = 0; s < 256; ++s) { const double v = db.getSample (ch, s); finite &= std::isfinite (v); e += v * v; }
            }
            check (finite && e > 0.0, "double-precision processBlock renders finite, non-silent audio");
            if (src) src->stop();
            // bypass with a non-zero reported latency must delay the dry input by exactly that latency
            proc->setLatencySamples (300);
            juce::AudioBuffer<float> bb (2, 512);
            bb.clear(); bb.setSample (0, 0, 1.0f); bb.setSample (1, 0, 1.0f);
            proc->processBlockBypassed (bb, midi);
            check (std::abs (bb.getSample (0, 300) - 1.0f) < 1.0e-6f && std::abs (bb.getSample (0, 0)) < 1.0e-6f, "host bypass delays dry by the reported latency");
            proc->setLatencySamples (0);
            const double tail = proc->getTailLengthSeconds();
            check (std::isfinite (tail) && tail >= 120.0, "tail policy: non-VST3 wrappers report a finite " + juce::String (tail, 0) + " s tail (VST3 reports infinite)");
        }

        // 3. state roundtrip into a second instance
        juce::MemoryBlock state;
        pp->setValue (BbdFeedback, 61.0f); pp->setValue (IvTap2Semi, -5.0f); pp->setValue (WaDecay, 77.0f);
        proc->getStateInformation (state);
        auto proc2 = juce::createPluginFilterOfType (juce::AudioProcessor::wrapperType_Standalone);
        auto* p2 = dynamic_cast<PluginProcessor*> (proc2.get());
        proc2->setStateInformation (state.getData(), (int) state.getSize());
        int mismatches = 0;
        for (int i = 0; i < kNumParams; ++i) if (std::abs (p2->value (i) - pp->value (i)) > 1.0e-3f && i != Freeze) ++mismatches;
        check (mismatches == 0, "session state roundtrip restores every parameter (" + juce::String (mismatches) + " mismatches)");

        // 3b. A/B: B gets different values, switching back and forth restores both complete states
        {
            auto& pm = pp->presets();
            pp->setValue (PlDecay, 5.0f); pp->setValue (HarmMethod, 2.0f); pp->setValue (IvTap3Semi, -7.0f);
            pm.switchAB (1);
            pp->setValue (PlDecay, 17.0f); pp->setValue (HarmMethod, 3.0f); pp->setValue (IvTap3Semi, 5.0f);
            pm.switchAB (0);
            const bool aOk = std::abs (pp->value (PlDecay) - 5.0f) < 0.01f && (int) pp->value (HarmMethod) == 2 && (int) pp->value (IvTap3Semi) == -7;
            pm.switchAB (1);
            const bool bOk = std::abs (pp->value (PlDecay) - 17.0f) < 0.01f && (int) pp->value (HarmMethod) == 3 && (int) pp->value (IvTap3Semi) == 5;
            pm.switchAB (0);
            check (aOk && bOk, "A/B switching restores complete states (including inactive-mode values)");
        }

        // 4. user preset save/load roundtrip (uniquely named, removed afterwards)
        {
            const auto name = "Selftest " + juce::String (juce::Time::currentTimeMillis());
            juce::String err;
            pp->setValue (PlDecay, 13.0f);
            const bool saved = pp->presets().saveUser (name, PresetManager::Collision::KeepBoth, err);
            pp->setValue (PlDecay, 3.0f);
            const auto file = PresetManager::userPresetDirectory().getChildFile (juce::File::createLegalFileName (name) + ".papreset");
            const bool loaded = pp->presets().loadFile (file, err);
            check (saved && loaded && std::abs (pp->value (PlDecay) - 13.0f) < 0.01f, "user preset atomic save + load roundtrip");
            file.deleteFile();
            pp->presets().rescan();
        }

        // 5. offline audition export (same engine)
        {
            AuditionExporter::Job job;
            job.params = pp->currentParams();
            job.sampleRate = 48000.0;
            std::vector<float> l, r;
            generateFixture (Fixture::BreathNoise, 48000.0, l, r);
            job.source.setSize (2, (int) l.size());
            job.source.copyFrom (0, 0, l.data(), (int) l.size());
            job.source.copyFrom (1, 0, r.data(), (int) r.size());
            job.events = builtInExperiments()[1].events;
            job.tailSeconds = 3.0;
            juce::AudioBuffer<float> out;
            const bool ok = AuditionExporter::renderToBuffer (job, out, nullptr, nullptr);
            check (ok && rms (out, 0, out.getNumSamples()) > 1.0e-4, "offline audition render (Freeze/Revoice sequence)");
        }

        // 6. editor construction and screenshots
        {
            pp->presets().loadFactory (0);
            if (src) { src->selectDemo ((int) Fixture::HarmonicTone); src->play(); }
            for (int b = 0; b < 48000 * 3 / 256; ++b) { buf.clear(); proc->processBlock (buf, midi); }
            std::unique_ptr<juce::AudioProcessorEditor> ed (pp->createEditor());
            check (ed != nullptr, "editor constructed");
            if (ed != nullptr && screenshotDir != juce::File())
            {
                screenshotDir.createDirectory();
                struct Shot { int w, h; float scale; const char* name; };
                const int iw = ed->getWidth(), ih = ed->getHeight();
                for (auto s : { Shot { iw, ih, 1.0f, "initial" }, Shot { 950, 680, 1.0f, "minimum-950x680" },
                                Shot { iw, ih, 2.0f, "retina-2x" }, Shot { 1600, 1040, 1.0f, "large-1600x1040" } })
                {
                    ed->setSize (s.w, s.h);
                    if (auto* pe = dynamic_cast<PluginEditor*> (ed.get()))
                    {
                        pe->refreshForSnapshot();
                        // run ~8 s of audio with UI frames so the rolling displays hold real history
                        for (int k = 0; k < 200; ++k)
                        {
                            for (int b = 0; b < 8; ++b) { buf.clear(); proc->processBlock (buf, midi); }
                            pe->tickForSnapshot();
                        }
                    }
                    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, s.scale);
                    juce::PNGImageFormat png;
                    auto f = screenshotDir.getChildFile (juce::String (s.name) + ".png");
                    f.deleteFile();
                    juce::FileOutputStream os (f);
                    const bool wrote = os.openedOk() && png.writeImageToStream (img, os);
                    check (wrote, "screenshot " + f.getFileName());
                }
                if (auto* pe = dynamic_cast<PluginEditor*> (ed.get()))
                {
                    struct State { const char* name; std::vector<std::pair<int, float>> v; };
                    const std::vector<State> states = {
                        { "state-interval-plate-chord", { { DelayMode, 1 }, { ReverbMode, 0 }, { NoteSource, 1 }, { HarmMethod, 2 }, { IvPitchMode, 1 } } },
                        { "state-delay-off-intervals", { { DelayEnable, 0 }, { NoteSource, 2 }, { HarmMethod, 3 } } },
                        { "state-arp-shift-series", { { DelayEnable, 1 }, { NoteSource, 3 }, { HarmMethod, 4 }, { Routing, 1 }, { Placement, 1 } } },
                    };
                    ed->setSize (iw, ih);
                    for (const auto& stt : states)
                    {
                        for (auto& [i, v] : stt.v) pp->setValue (i, v);
                        for (int k = 0; k < 120; ++k) { for (int b = 0; b < 8; ++b) { buf.clear(); proc->processBlock (buf, midi); } pe->tickForSnapshot(); }
                        auto im = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                        juce::FileOutputStream os3 (screenshotDir.getChildFile (juce::String (stt.name) + ".png"));
                        if (os3.openedOk()) { os3.setPosition (0); os3.truncate(); juce::PNGImageFormat().writeImageToStream (im, os3); }
                    }
                    pp->presets().loadFactory (0);
                    ed->setSize (iw, ih);
                    for (auto* tab : { "Harmony", "Delay", "MIDI", "Mix / Timing", "Diagnostics" })
                    {
                        pe->showAdvancedForSnapshot (tab);
                        auto im = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                        juce::FileOutputStream os2 (screenshotDir.getChildFile ("advanced-" + juce::File::createLegalFileName (juce::String (tab).replace (" / ", "-")) + ".png"));
                        if (os2.openedOk()) { os2.setPosition (0); os2.truncate(); juce::PNGImageFormat().writeImageToStream (im, os2); }
                    }
                    pe->showAdvancedForSnapshot ("Harmony");
                    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                    juce::FileOutputStream os (screenshotDir.getChildFile ("advanced-panel.png"));
                    juce::PNGImageFormat().writeImageToStream (img, os);
                    pe->showAdvancedForSnapshot ({});
                }
            }
        }
        log.add (failures == 0 ? "SELFTEST PASSED" : "SELFTEST FAILED (" + juce::String (failures) + ")");
    }
};
} // namespace

// ------------------------------------------------------------------------------------------ main window
class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (juce::AudioProcessor& p)
        : juce::DocumentWindow ("Playable Ambience", juce::Colour (0xffeae5da), juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, false);
        setContentOwned (p.createEditorIfNeeded(), true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class StandaloneApp : public juce::JUCEApplication, public StandaloneHost
{
public:
    const juce::String getApplicationName() override { return "Playable Ambience"; }
    const juce::String getApplicationVersion() override { return PA_BUILD_VERSION; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String& commandLine) override
    {
        const auto args = juce::StringArray::fromTokens (commandLine, true);
        if (args.contains ("--selftest"))
        {
            juce::File shots;
            const int si = args.indexOf ("--screenshots");
            if (si >= 0 && si + 1 < args.size()) shots = juce::File::getCurrentWorkingDirectory().getChildFile (args[si + 1].unquoted());
            SelfTest t;
            t.run (shots);
            const auto text = t.log.joinIntoString ("\n");
            std::fputs ((text + "\n").toRawUTF8(), stdout);
            const int ri = args.indexOf ("--report");
            if (ri >= 0 && ri + 1 < args.size()) juce::File::getCurrentWorkingDirectory().getChildFile (args[ri + 1].unquoted()).replaceWithText (text + "\n");
            setApplicationReturnValue (t.failures == 0 ? 0 : 1);
            quit();
            return;
        }

        appProperties.setStorageParameters (propertyOptions());
        StandaloneHost::instance() = this;
        processor = juce::createPluginFilterOfType (juce::AudioProcessor::wrapperType_Standalone);

        auto* props = appProperties.getUserSettings();
        auto savedDevice = props->getXmlValue ("audioDevice");
        // Start with NO input channels: the microphone permission prompt only appears when Live Input is enabled.
        deviceManager.initialise (0, 2, savedDevice.get(), true);
        auto setup = deviceManager.getAudioDeviceSetup();
        if (setup.inputChannels.countNumberOfSetBits() > 0)
        {
            setup.inputChannels.clear(); setup.useDefaultInputChannels = false;
            deviceManager.setAudioDeviceSetup (setup, true);
        }
        // MIDI: restore enabled inputs by identifier, or enable everything on first launch
        const auto savedMidi = juce::StringArray::fromTokens (props->getValue ("midiInputs", "*"), "\n", "");
        for (auto& d : juce::MidiInput::getAvailableDevices())
            deviceManager.setMidiInputDeviceEnabled (d.identifier, savedMidi.contains ("*") || savedMidi.contains (d.identifier) || savedMidi.contains (d.name));
        deviceManager.addMidiInputDeviceCallback ({}, &player);

        juce::MemoryBlock state;
        if (state.fromBase64Encoding (props->getValue ("pluginState")) && state.getSize() > 0)
            processor->setStateInformation (state.getData(), (int) state.getSize());

        player.setProcessor (processor.get());
        deviceManager.addAudioCallback (&player);
        window = std::make_unique<MainWindow> (*processor);
    }

    void shutdown() override
    {
        if (processor == nullptr) return;
        saveSettings();
        deviceManager.removeAudioCallback (&player);
        deviceManager.removeMidiInputDeviceCallback ({}, &player);
        player.setProcessor (nullptr);
        window.reset();
        processor.reset();
        StandaloneHost::instance() = nullptr;
    }

    void systemRequestedQuit() override { quit(); }

    // StandaloneHost
    void showAudioMidiSettings() override
    {
        auto* sel = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 2, 2, 2, true, false, true, false);
        sel->setSize (520, 560);
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (sel);
        o.dialogTitle = "Audio / MIDI Settings";
        o.dialogBackgroundColour = juce::Colour (0xfff6f1e8);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = true;
        o.launchAsync();
    }

    void setLiveInput (bool enabled) override
    {
        auto* pp = dynamic_cast<PluginProcessor*> (processor.get());
        if (! enabled)
        {
            if (pp) pp->setLiveInputEnabled (false);
            auto setup = deviceManager.getAudioDeviceSetup();
            setup.inputChannels.clear(); setup.useDefaultInputChannels = false;
            deviceManager.setAudioDeviceSetup (setup, true);
            return;
        }
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio, [this, pp] (bool granted) {
            if (! granted) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Live Input", "Microphone access was not granted. You can allow it in System Settings > Privacy & Security > Microphone."); return; }
            auto setup = deviceManager.getAudioDeviceSetup();
            if (setup.inputChannels.countNumberOfSetBits() == 0) { setup.inputChannels.setRange (0, 2, true); setup.useDefaultInputChannels = false; }
            deviceManager.setAudioDeviceSetup (setup, true);
            if (pp) pp->setLiveInputEnabled (true);
        });
    }

    bool liveInputActive() const override
    {
        auto* pp = dynamic_cast<PluginProcessor*> (processor.get());
        return pp != nullptr && pp->liveInputEnabled();
    }

    juce::String deviceSummary() const override
    {
        if (auto* d = deviceManager.getCurrentAudioDevice())
            return d->getName() + "  " + juce::String (d->getCurrentSampleRate(), 0) + " Hz / " + juce::String (d->getCurrentBufferSizeSamples())
                   + "  in " + juce::String (d->getActiveInputChannels().countNumberOfSetBits()) + " out " + juce::String (d->getActiveOutputChannels().countNumberOfSetBits());
        return "No audio device";
    }

    juce::StringArray enabledMidiInputs() const override
    {
        juce::StringArray s;
        for (auto& d : juce::MidiInput::getAvailableDevices())
            if (deviceManager.isMidiInputDeviceEnabled (d.identifier)) s.add (d.name);
        return s;
    }

private:
    void saveSettings()
    {
        auto* props = appProperties.getUserSettings();
        if (auto xml = deviceManager.createStateXml()) props->setValue ("audioDevice", xml.get());
        juce::StringArray midi;
        for (auto& d : juce::MidiInput::getAvailableDevices())
            if (deviceManager.isMidiInputDeviceEnabled (d.identifier)) midi.add (d.identifier);
        props->setValue ("midiInputs", midi.isEmpty() ? juce::String ("none") : midi.joinIntoString ("\n"));
        juce::MemoryBlock state;
        processor->getStateInformation (state);
        props->setValue ("pluginState", state.toBase64Encoding());
        props->saveIfNeeded();
    }

    juce::ApplicationProperties appProperties;
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer player;
    std::unique_ptr<juce::AudioProcessor> processor;
    std::unique_ptr<MainWindow> window;
};
} // namespace pa

juce::JUCEApplicationBase* juce_CreateApplication() { return new pa::StandaloneApp(); }

#endif
