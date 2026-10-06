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

        // 3a. legacy session (before the Classic-only build): Off -> Harmony Enable off, FFT -> Classic, no mix -> 50 %
        {
            auto legacy = juce::createPluginFilterOfType (juce::AudioProcessor::wrapperType_Standalone);
            auto* lp = dynamic_cast<PluginProcessor*> (legacy.get());
            for (int m : { 0, 2 })
            {
                juce::XmlElement x ("PlayableAmbience");
                auto* pe = x.createNewChildElement ("PARAMS");
                auto* e1 = pe->createNewChildElement ("P"); e1->setAttribute ("id", "harmMethod"); e1->setAttribute ("v", m);
                auto* e2 = pe->createNewChildElement ("P"); e2->setAttribute ("id", "depth"); e2->setAttribute ("v", 77.0);
                juce::MemoryBlock mb;
                juce::AudioProcessor::copyXmlToBinary (x, mb);
                lp->setStateInformation (mb.getData(), (int) mb.getSize());
                const bool ok = (int) lp->value (HarmMethod) == 1 && (lp->value (HarmEnable) > 0.5f) == (m != 0) && std::abs (lp->value (Mix) - 50.0f) < 0.01f
                                && std::abs (lp->value (Depth) - 77.0f) < 0.01f;
                check (ok, juce::String ("legacy session with method ") + (m == 0 ? "Off" : "FFT") + " loads as " + (m == 0 ? "Harmony off" : "Classic") + ", Dry/Wet 50 %");
            }
        }

        // 3b. A/B: B gets different values, switching back and forth restores both complete states
        {
            auto& pm = pp->presets();
            pp->setValue (PlDecay, 5.0f); pp->setValue (ClBands, 2.0f); pp->setValue (IvTap3Semi, -7.0f);
            pm.switchAB (1);
            pp->setValue (PlDecay, 17.0f); pp->setValue (ClBands, 0.0f); pp->setValue (IvTap3Semi, 5.0f);
            pm.switchAB (0);
            const bool aOk = std::abs (pp->value (PlDecay) - 5.0f) < 0.01f && (int) pp->value (ClBands) == 2 && (int) pp->value (IvTap3Semi) == -7;
            pm.switchAB (1);
            const bool bOk = std::abs (pp->value (PlDecay) - 17.0f) < 0.01f && (int) pp->value (ClBands) == 0 && (int) pp->value (IvTap3Semi) == 5;
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
            auto* pe = dynamic_cast<PluginEditor*> (ed.get());
            if (pe != nullptr && screenshotDir != juce::File())
            {
                screenshotDir.createDirectory();
                auto save = [&] (const juce::Image& img, const juce::String& name) {
                    auto f = screenshotDir.getChildFile (name + ".png");
                    f.deleteFile();
                    juce::FileOutputStream os (f);
                    const bool wrote = os.openedOk() && juce::PNGImageFormat().writeImageToStream (img, os);
                    check (wrote, "screenshot " + f.getFileName());
                };
                auto run = [&] (int frames) { for (int k = 0; k < frames; ++k) { for (int b = 0; b < 8; ++b) { buf.clear(); proc->processBlock (buf, midi); } pe->tickForSnapshot(); } };

                // (a) reference visual fixture: the approved state, deterministic graph data, canonical 1536 x 1024
                {
                    if (src) src->stop();
                    pp->presets().loadFactory (0);
                    pp->presets().switchAB (0);
                    for (auto [i, v] : std::initializer_list<std::pair<int, float>> {
                             { DelayEnable, 1 }, { DelayMode, 0 }, { BbdSync, 1 }, { BbdDiv, 8 }, { BbdFeedback, 42 }, { BbdTone, 3200 }, { BbdAge, 28 }, { BbdLevel, -8 },
                             { ReverbEnable, 1 }, { ReverbMode, 1 }, { WaDecay, 24 }, { WaBloom, 65 }, { WaTone, 4800 }, { WaMotion, 30 }, { WaLevel, -6 },
                             { Routing, 0 }, { Placement, 0 }, { HarmEnable, 1 }, { HarmMethod, 1 }, { NoteSource, 1 }, { ChRoot, 0 }, { ChQuality, 1 }, { ChOctave, 3 },
                             { ChInversion, 0 }, { ChSpread, 0 }, { NoNotePolicy, 0 }, { Depth, 68 }, { Colour, 35 }, { Transition, 180 }, { DuckAmount, 24 },
                             { Latch, 1 }, { Freeze, 0 }, { WetOnly, 0 }, { Mix, 35 } })
                        pp->setValue (i, v);
                    run (30); // voices settle on C3 Eb3 G3 (stored chord), silence in
                    pe->setReferenceFixture (true);
                    pe->refreshForSnapshot();
                    save (pe->snapshotCanvas (1.0f), "reference-fixture-1536x1024");
                    save (pe->snapshotCanvas (2.0f), "reference-fixture-2x");
                    pe->setReferenceFixture (false);
                }
                // (b) live processing fixture: deterministic demo audio + chord changes through the real processor
                {
                    pp->presets().loadFactory (0);
                    pp->setValue (NoteSource, 1);
                    if (src) { src->selectDemo ((int) Fixture::HarmonicTone); src->play(); }
                    run (45);
                    save (pe->snapshotCanvas (1.0f), "live-playing");
                    pp->setValue (ChRoot, 8); pp->setValue (ChQuality, 0); // A-flat major over the tail
                    run (60);
                    save (pe->snapshotCanvas (1.0f), "live-tail-revoiced");
                    pp->setValue (Freeze, 1);
                    run (60);
                    save (pe->snapshotCanvas (1.0f), "live-freeze");
                    pp->setValue (Freeze, 0);
                    if (src) src->stop();
                    pp->tailKill();
                    run (200);
                    save (pe->snapshotCanvas (1.0f), "live-silence");
                    if (src) { src->selectDemo ((int) Fixture::HarmonicTone); src->play(); }
                    pp->setValue (NoteSource, 3);
                    pp->setValue (DelayMode, 1); pp->setValue (ReverbMode, 0); pp->setValue (Routing, 1);
                    run (90);
                    save (pe->snapshotCanvas (1.0f), "live-arp-interval-plate");
                    pp->setValue (NoteSource, 0); pp->setValue (Placement, 1); pp->setValue (HarmEnable, 0);
                    run (40);
                    save (pe->snapshotCanvas (1.0f), "live-harmony-off-before-space");
                    pp->setValue (HarmEnable, 1);
                }
                // (a2) open menu state: the warm popup drawn by the editor's look-and-feel (Source dropdown items)
                {
                    auto& lf = pe->getLookAndFeel();
                    const juce::StringArray items { "MIDI", "Chord", "Intervals", "Arp" };
                    const int w = 180, ih = 28, h = ih * items.size() + 10;
                    for (float sc : { 1.0f, 2.0f })
                    {
                        juce::Image img (juce::Image::ARGB, (int) (w * sc), (int) (h * sc), true);
                        juce::Graphics g (img);
                        g.addTransform (juce::AffineTransform::scale (sc));
                        lf.drawPopupMenuBackground (g, w, h);
                        for (int k = 0; k < items.size(); ++k)
                            lf.drawPopupMenuItem (g, { 0, 5 + k * ih, w, ih }, false, true, k == 2, k == 1, false, items[k], {}, nullptr, nullptr);
                        save (img, sc > 1.5f ? "menu-source-open-2x" : "menu-source-open");
                    }
                }
                // (b2) UI cost: full main-view repaint with live graph data (software renderer, 1x and 2x)
                {
                    for (float sc : { 1.0f, 2.0f })
                    {
                        const auto t0 = juce::Time::getMillisecondCounterHiRes();
                        constexpr int frames = 20;
                        for (int k = 0; k < frames; ++k) { for (int b = 0; b < 8; ++b) { buf.clear(); proc->processBlock (buf, midi); } pe->tickForSnapshot(); auto im = pe->snapshotCanvas (sc); juce::ignoreUnused (im); }
                        const double ms = (juce::Time::getMillisecondCounterHiRes() - t0) / frames;
                        log.add ("      UI worst case (audio + update + full-canvas repaint, " + juce::String (sc, 0) + "x): " + juce::String (ms, 1) + " ms");
                        {
                            double tp = 0, tu = 0, ts = 0;
                            for (int k = 0; k < frames; ++k)
                            {
                                auto a0 = juce::Time::getMillisecondCounterHiRes();
                                for (int b = 0; b < 8; ++b) { buf.clear(); proc->processBlock (buf, midi); }
                                auto a1 = juce::Time::getMillisecondCounterHiRes();
                                pe->tickForSnapshot();
                                auto a2 = juce::Time::getMillisecondCounterHiRes();
                                auto im = pe->snapshotCanvas (sc); juce::ignoreUnused (im);
                                auto a3 = juce::Time::getMillisecondCounterHiRes();
                                tp += a1 - a0; tu += a2 - a1; ts += a3 - a2;
                            }
                            GraphWell::profDraw = GraphWell::profRender = GraphWell::profBlit = 0;
                            std::vector<std::pair<double, juce::String>> costs;
                            for (auto* c : pe->mainView()->getChildren())
                            {
                                if (! c->isVisible()) continue; // closed settings views cost nothing
                                auto b0 = juce::Time::getMillisecondCounterHiRes();
                                for (int r = 0; r < 5; ++r) { if (auto* gw = dynamic_cast<GraphWell*> (c)) gw->markDirty(); auto im = c->createComponentSnapshot (c->getLocalBounds(), true, sc); juce::ignoreUnused (im); }
                                costs.push_back ({ (juce::Time::getMillisecondCounterHiRes() - b0) / 5.0, c->getTitle() + "/" + c->getName() + " " + c->getBounds().toString() });
                            }
                            std::sort (costs.begin(), costs.end(), [] (auto& a, auto& b) { return a.first > b.first; });
                            double tot = 0; for (auto& cc : costs) tot += cc.first;
                            log.add ("      children total " + juce::String (tot, 1) + " ms; graphs draw " + juce::String (GraphWell::profDraw / 15.0, 2) + " render " + juce::String (GraphWell::profRender / 15.0, 2) + " blit " + juce::String (GraphWell::profBlit / 15.0, 2) + " ms per graph");
                            for (int q = 0; q < 4 && q < (int) costs.size(); ++q) log.add ("        " + juce::String (costs[(size_t) q].first, 2) + " ms  " + costs[(size_t) q].second);
                            log.add ("      profile: audio " + juce::String (tp / frames, 2) + " ms, update " + juce::String (tu / frames, 2) + " ms, paint " + juce::String (ts / frames, 2) + " ms");
                        }
                    }
                }
                // (c) whole editor at the default, minimum, retina and large sizes (host-style window content)
                {
                    pp->presets().loadFactory (0);
                    if (src) { src->selectDemo ((int) Fixture::HarmonicTone); src->play(); }
                    const int iw = ed->getWidth(), ih = ed->getHeight();
                    struct Shot { int w, h; float scale; const char* name; };
                    const float aspect = (float) iw / (float) ih;
                    const int minW = (int) std::ceil (theme::kCanvasW * PluginEditor::kMinScale);
                    for (auto sh : { Shot { iw, ih, 1.0f, "initial" }, Shot { minW, (int) std::lround (minW / aspect), 1.0f, "minimum" },
                                     Shot { iw, ih, 2.0f, "retina-2x" }, Shot { 1843, (int) std::lround (1843 / aspect), 1.0f, "large-120pct" } })
                    {
                        ed->setSize (sh.w, sh.h);
                        pe->refreshForSnapshot();
                        run (80);
                        save (ed->createComponentSnapshot (ed->getLocalBounds(), true, sh.scale), sh.name);
                    }
                    ed->setSize (iw, ih);
                    pe->refreshForSnapshot();
                    // contextual effect settings (canonical canvas) for every mode / source / page
                    struct DetailShot { int effect, page; std::vector<std::pair<int, float>> set; const char* name; };
                    const std::vector<DetailShot> shots {
                        { 0, 0, { { DelayMode, 0 } }, "detail-bbd" },
                        { 0, 0, { { DelayMode, 1 } }, "detail-interval-taps" },
                        { 0, 1, { { DelayMode, 1 } }, "detail-interval-pitch" },
                        { 0, 2, { { DelayMode, 1 } }, "detail-interval-character" },
                        { 1, 0, { { DelayMode, 0 }, { ReverbMode, 0 } }, "detail-plate" },
                        { 1, 0, { { ReverbMode, 1 } }, "detail-wash" },
                        { 1, 0, { { ReverbMode, 2 } }, "detail-hall" },
                        { 1, 1, { { ReverbMode, 2 }, { Shimmer, 45 } }, "detail-reverb-shimmer" },
                        { 0, 0, { { DelayMode, 2 } }, "detail-tape" },
                        { 2, 0, { { NoteSource, 1 } }, "detail-harmony-chord" },
                        { 2, 0, { { NoteSource, 2 } }, "detail-harmony-intervals" },
                        { 2, 0, { { NoteSource, 3 }, { ArpSync, 1 } }, "detail-harmony-arp" },
                        { 2, 0, { { NoteSource, 0 } }, "detail-harmony-midi" },
                        { 2, 1, { { NoteSource, 1 } }, "detail-harmony-voice" },
                        { 2, 2, { { NoteSource, 1 } }, "detail-harmony-vocoder" } };
                    for (auto& sh : shots)
                    {
                        for (auto [i, v] : sh.set) pp->setValue (i, v);
                        run (2);
                        pe->showEffectSettings (sh.effect, sh.page);
                        run (3);
                        save (pe->snapshotCanvas (1.0f), sh.name);
                    }
                    save (pe->snapshotCanvas (2.0f), "detail-harmony-vocoder-2x");
                    pe->showEffectSettings (-1);
                    run (3);
                    save (pe->snapshotCanvas (1.0f), "detail-closed-graphs-live");
                    for (int pg = 0; pg < 3; ++pg)
                    {
                        pe->showSettings (pg);
                        run (2);
                        save (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), juce::String ("settings-") + juce::StringArray { "midi", "audio", "support" }[pg]);
                    }
                    save (ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f), "settings-support-2x");
                    pe->showSettings (-1);
                }
            }
            // 6b. contextual settings: one effect view at a time, mode-following content, Escape / outside click behaviour
            if (pe != nullptr)
            {
                auto tick = [&] (int frames) { for (int k = 0; k < frames; ++k) { for (int b = 0; b < 8; ++b) { buf.clear(); proc->processBlock (buf, midi); } pe->tickForSnapshot(); } };
                pp->setValue (ReverbMode, 0);
                pe->showEffectSettings (1);
                tick (2);
                const bool plate = pe->openEffectSettings() == 1 && pe->effectSettingsTitle (1) == "PLATE SETTINGS";
                pp->setValue (ReverbMode, 1);
                tick (2);
                const bool wash = pe->effectSettingsTitle (1) == "WASH SETTINGS";
                pe->showEffectSettings (0);
                const bool one = pe->openEffectSettings() == 0;
                pp->setValue (DelayMode, 1);
                tick (2);
                const bool interval = pe->effectSettingsTitle (0) == "INTERVAL SETTINGS";
                pp->setValue (DelayMode, 0);
                tick (2);
                pe->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
                const bool esc = pe->openEffectSettings() == -1;
                check (plate && wash && one && interval && esc, "effect settings: Plate -> Wash and BBD -> Interval follow the mode, one open at a time, Escape returns to the graph");
                // A/B switch while a settings view is open: the view follows the recalled mode, the graph comes back on close
                {
                    auto& pm = pp->presets();
                    pm.switchAB (0); pp->setValue (DelayMode, 0);
                    pm.switchAB (1); pp->setValue (DelayMode, 1);
                    pm.switchAB (0);
                    pe->showEffectSettings (0);
                    tick (2);
                    const bool a = pe->effectSettingsTitle (0) == "BBD SETTINGS";
                    pm.switchAB (1);
                    tick (2);
                    const bool b = pe->effectSettingsTitle (0) == "INTERVAL SETTINGS";
                    pm.switchAB (0);
                    pe->showEffectSettings (-1);
                    check (a && b && pe->openEffectSettings() == -1, "A/B recall while Delay settings are open: the view follows the slot's mode");
                }
                pe->showSettings (1);
                // a point over the Dry/Wet knob (canvas 1450, 62) and one over the Feedback knob must hit the outside-click layer
                const float sc = (float) ed->getWidth() / (float) theme::kCanvasW;
                const int top = ed->getHeight() - (int) std::lround (theme::kCanvasH * sc);
                const float mixBefore = pp->value (Mix);
                const bool wasVisible = ed->isVisible();
                ed->setVisible (true); // hit testing only considers visible components (the editor has no window here)
                auto* hitA = ed->getComponentAt (juce::Point<int> ((int) (245 * sc), top + (int) (459 * sc)));
                auto* hitB = ed->getComponentAt (juce::Point<int> ((int) (1450 * sc), top + (int) (62 * sc)));
                const bool blocked = hitA != nullptr && hitA->getName() == "settings-outside-click" && (hitB == nullptr || hitB->getName() == "settings-outside-click"
                                                                                                       || hitB->findParentComponentOfClass<SettingsPanel>() != nullptr);
                ed->setVisible (wasVisible);
                if (! blocked) log.add ("      hit A: " + (hitA ? juce::String (typeid (*hitA).name()) + " '" + hitA->getName() + "' " + hitA->getBounds().toString() : juce::String ("none"))
                                        + "  hit B: " + (hitB ? juce::String (typeid (*hitB).name()) + " '" + hitB->getName() + "'" : juce::String ("none")) + " open " + juce::String ((int) pe->settingsOpen()));
                pe->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
                check (blocked && ! pe->settingsOpen() && pp->value (Mix) == mixBefore, "Settings panel: outside clicks are caught (no obscured parameter changes), Escape closes it");
                pe->showDestination ("Mix / Timing");
                const bool routed = pe->settingsOpen();
                pe->showDestination ("Harmony");
                check (routed && ! pe->settingsOpen() && pe->openEffectSettings() == 2, "retired Advanced destinations route to Settings / Harmony settings");
                pe->showDestination ({});
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
