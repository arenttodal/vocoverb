// Host-side MIDI delivery check. Loads a built plug-in binary the way a DAW does (JUCE hosting: VST3, and AU on
// macOS), reports whether it exposes a MIDI input, then renders the same audio twice through fresh instances:
// once without MIDI and once with a held MIDI chord. If MIDI reaches the harmony engine the outputs differ.
//
//   pa_hostcheck <plugin path or AU identifier> [--expect-midi | --expect-no-midi]
//
// Exit code 0 = expectation met. Output is plain text for CI logs.
#include <JuceHeader.h>

#include <cmath>
#include <cstdio>

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 512;
constexpr int kSeconds = 4;

std::unique_ptr<juce::AudioPluginInstance> load (juce::AudioPluginFormatManager& fm, const juce::String& path, juce::String& err)
{
    for (auto* f : fm.getFormats())
    {
        juce::OwnedArray<juce::PluginDescription> types;
        f->findAllTypesForFile (types, path);
        for (auto* d : types)
        {
            std::printf ("  found: %s (%s) %s\n", d->name.toRawUTF8(), d->pluginFormatName.toRawUTF8(), d->fileOrIdentifier.toRawUTF8());
            if (auto inst = fm.createPluginInstance (*d, kRate, kBlock, err)) return inst;
        }
    }
    if (err.isEmpty()) err = "no plug-in found in " + path;
    return nullptr;
}

// Voice-like source: a sawtooth phrase (A2 -> C3) with vibrato for 2 s, then silence so the tail is heard.
juce::AudioBuffer<float> makeSource()
{
    const int n = (int) (kRate * kSeconds);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    double ph = 0.0;
    for (int i = 0; i < (int) (kRate * 2.0); ++i)
    {
        const double t = i / kRate;
        const double f = (t < 1.0 ? 110.0 : 130.81) * (1.0 + 0.004 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.0 * t));
        ph += f / kRate; ph -= std::floor (ph);
        const double env = std::min (1.0, t * 20.0) * std::min (1.0, (2.0 - t) * 20.0);
        const float v = (float) (0.25 * env * (2.0 * ph - 1.0));
        b.setSample (0, i, v); b.setSample (1, i, v);
    }
    return b;
}

struct RunResult { juce::AudioBuffer<float> out; bool ok = false; };

RunResult render (juce::AudioPluginFormatManager& fm, const juce::String& path, bool sendMidi, juce::String& err)
{
    RunResult r;
    auto inst = load (fm, path, err);
    if (! inst) return r;
    // plain stereo in / out (disable any sidechain bus as a DAW insert would)
    auto layout = inst->getBusesLayout();
    for (int i = 1; i < layout.inputBuses.size(); ++i) layout.inputBuses.getReference (i) = juce::AudioChannelSet::disabled();
    if (! inst->setBusesLayout (layout)) std::printf ("  note: host could not disable non-main buses; keeping default layout\n");
    inst->setRateAndBufferSizeDetails (kRate, kBlock);
    inst->prepareToPlay (kRate, kBlock);
    const auto src = makeSource();
    const int n = src.getNumSamples();
    const int chans = std::max (inst->getTotalNumInputChannels(), inst->getTotalNumOutputChannels());
    r.out.setSize (2, n);
    r.out.clear();
    juce::AudioBuffer<float> block (std::max (2, chans), kBlock);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < n; pos += kBlock)
    {
        const int len = std::min (kBlock, n - pos);
        block.clear();
        for (int ch = 0; ch < 2; ++ch) block.copyFrom (ch, 0, src, ch, pos, len);
        midi.clear();
        if (sendMidi && pos == kBlock * 4)
        {
            // F# major (F#3 A#3 C#4): far from the presets' stored C minor fallback chord
            for (int note : { 54, 58, 61 }) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 10);
        }
        juce::AudioBuffer<float> view (block.getArrayOfWritePointers(), block.getNumChannels(), len);
        inst->processBlock (view, midi);
        for (int ch = 0; ch < 2; ++ch) r.out.copyFrom (ch, pos, view, ch, 0, len);
    }
    inst->releaseResources();
    r.ok = true;
    return r;
}

double rmsDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double s = 0.0; int c = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i) { const double d = a.getSample (ch, i) - b.getSample (ch, i); s += d * d; ++c; }
    return std::sqrt (s / std::max (1, c));
}

double rms (const juce::AudioBuffer<float>& a)
{
    double s = 0.0; int c = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i) { const double d = a.getSample (ch, i); s += d * d; ++c; }
    return std::sqrt (s / std::max (1, c));
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::printf ("usage: pa_hostcheck <plugin> [--expect-midi|--expect-no-midi]\n"); return 2; }
    const juce::String path = juce::CharPointer_UTF8 (argv[1]);
    const juce::String mode = argc > 2 ? juce::String (argv[2]) : juce::String ("--expect-midi");
    const bool expectMidi = mode != "--expect-no-midi";

    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager (fm);
    std::printf ("HOST MIDI CHECK: %s (expect %s)\n", path.toRawUTF8(), expectMidi ? "MIDI input" : "no MIDI input");

    juce::String err;
    auto probe = load (fm, path, err);
    if (! probe) { std::printf ("FAIL: could not load: %s\n", err.toRawUTF8()); return 1; }
    const bool accepts = probe->acceptsMidi();
    std::printf ("  name: %s  format: %s  acceptsMidi: %s  inputs: %d  outputs: %d  latency: %d\n",
                 probe->getName().toRawUTF8(), probe->getPluginDescription().pluginFormatName.toRawUTF8(), accepts ? "yes" : "no",
                 probe->getTotalNumInputChannels(), probe->getTotalNumOutputChannels(), probe->getLatencySamples());
    probe.reset();

    if (! expectMidi)
    {
        const bool pass = ! accepts;
        std::printf ("%s: plug-in %s a MIDI input\n", pass ? "PASS" : "FAIL", accepts ? "exposes" : "does not expose");
        return pass ? 0 : 1;
    }
    if (! accepts) { std::printf ("FAIL: host sees no MIDI input on this plug-in\n"); return 1; }

    auto a1 = render (fm, path, false, err);
    auto a2 = render (fm, path, false, err);
    auto b = render (fm, path, true, err);
    if (! a1.ok || ! a2.ok || ! b.ok) { std::printf ("FAIL: render: %s\n", err.toRawUTF8()); return 1; }
    const double level = rms (a1.out), noise = rmsDiff (a1.out, a2.out), effect = rmsDiff (a1.out, b.out);
    const double rel = effect / std::max (1.0e-12, level);
    std::printf ("  output rms %.6f  | run-to-run difference %.3g  | difference caused by MIDI %.6f (%.1f%% of output)\n", level, noise, effect, 100.0 * rel);
    const bool pass = level > 1.0e-4 && rel > 0.05 && effect > 10.0 * noise;
    std::printf ("%s: %s\n", pass ? "PASS" : "FAIL",
                 pass ? "MIDI notes sent by the host change the harmony output" : "MIDI sent by the host had no audible effect");
    return pass ? 0 : 1;
}
