#include "PluginProcessor.h"

#include "BuildInfo.h"
#include "Presets/PresetManager.h"
#include "Standalone/SourcePlayer.h"
#include "UI/PluginEditor.h"

#include <limits>

namespace pa
{
namespace
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& info = paramInfo (i);
        const juce::ParameterID pid { info.id, kParamVersion };
        auto toText = [i] (float v, int) { return juce::String (paramValueToText (i, v)); };
        auto fromText = [i] (const juce::String& s) {
            float v = paramInfo (i).def;
            paramTextToValue (i, s.toStdString(), v);
            return v;
        };
        switch (info.kind)
        {
            case Kind::Float:
            {
                juce::NormalisableRange<float> range (info.minV, info.maxV);
                if (info.centre > info.minV && info.centre < info.maxV) range.setSkewForCentre (info.centre);
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, info.name, range, info.def,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText).withValueFromStringFunction (fromText).withLabel (info.unit)));
                break;
            }
            case Kind::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, info.name, (int) info.minV, (int) info.maxV, (int) info.def,
                    juce::AudioParameterIntAttributes()
                        .withStringFromValueFunction ([i] (int v, int) { return juce::String (paramValueToText (i, (float) v)); })
                        .withValueFromStringFunction ([i] (const juce::String& s) { float v = paramInfo (i).def; paramTextToValue (i, s.toStdString(), v); return (int) v; })));
                break;
            case Kind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, info.name, info.def >= 0.5f));
                break;
            case Kind::Choice:
            {
                juce::StringArray ch;
                for (auto& c : paramChoices (i)) ch.add (c);
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, info.name, ch, (int) info.def));
                break;
            }
        }
    }
    return layout;
}

} // namespace

juce::AudioProcessor::BusesProperties PluginProcessor::makeBuses()
{
    return BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
        .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false);
}

PluginProcessor::PluginProcessor()
    : juce::AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
    {
        params[(size_t) i] = apvts.getParameter (paramInfo (i).id);
        raw[(size_t) i] = apvts.getRawParameterValue (paramInfo (i).id);
        jassert (params[(size_t) i] != nullptr && raw[(size_t) i] != nullptr);
    }
    snapshots = defaultSnapshots();
    presetManager = std::make_unique<PresetManager> (*this);
    if (wrapperType == wrapperType_Standalone)
    {
        sourcePlayer = std::make_unique<SourcePlayer>();
        if (auto* p = params[(size_t) Timing]) p->setValueNotifyingHost (p->convertTo0to1 (0.0f)); // Live in the standalone
    }
    for (auto& c : cpuRing) c.store (0.0f);
    events.reserve (4096);
    events.resize (4096);
    // Apply the first factory preset as the initial musical state.
    presetManager->loadFactory (0, false);
    startTimerHz (30);
}

PluginProcessor::~PluginProcessor()
{
    stopTimer();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono() && ! in.isDisabled()) return false;
    if (out == juce::AudioChannelSet::mono() && in == juce::AudioChannelSet::stereo()) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    preparedRate = sampleRate;
    preparedBlock = std::max (32, samplesPerBlock);
    const int slice = std::max (preparedBlock, 4096);
    engine.prepare (sampleRate, slice);
    inL.assign ((size_t) slice, 0.0f); inR.assign ((size_t) slice, 0.0f);
    outL.assign ((size_t) slice, 0.0f); outR.assign ((size_t) slice, 0.0f);
    srcL.assign ((size_t) slice, 0.0f); srcR.assign ((size_t) slice, 0.0f);
    for (auto& b : bypassLine) b.assign ((size_t) (engine.studioLatency (2) + 16), 0.0f);
    bypassWrite = 0;
    if (sourcePlayer) sourcePlayer->prepare (sampleRate, slice);
    appliedQuality = juce::jlimit (0, 2, (int) value (Quality));
    appliedTiming = (int) value (Timing);
    const int lat = appliedTiming == 1 ? engine.studioLatency (appliedQuality) : 0;
    desiredLatency.store (lat);
    setLatencySamples (lat);
    pendingLatency.store (false);
}

double PluginProcessor::getTailLengthSeconds() const
{
    // VST3: infinite tail so hosts never suspend a frozen or 120 s wash. AU: a finite bound above the longest decay.
    if (wrapperType == wrapperType_VST3) return std::numeric_limits<double>::infinity();
    return 130.0;
}

ParamSet PluginProcessor::currentParams() const
{
    ParamSet ps;
    for (int i = 0; i < kNumParams; ++i) ps[i] = raw[(size_t) i]->load();
    return ps;
}

void PluginProcessor::setValue (int index, float realValue)
{
    if (auto* p = params[(size_t) index])
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (paramSnap (index, realValue)));
        p->endChangeGesture();
    }
}

void PluginProcessor::applyParams (const ParamSet& psIn, bool includeHostLocal)
{
    ParamSet ps = psIn;
    migrateLegacyHarmony (ps);
    for (int i = 0; i < kNumParams; ++i)
    {
        if (! includeHostLocal && isHostLocalParam (i)) continue;
        if (i == Freeze) continue; // performance state, never recalled from presets
        if (std::abs (raw[(size_t) i]->load() - ps[i]) > 1.0e-6f) setValue (i, ps[i]);
    }
    if (raw[(size_t) Freeze]->load() > 0.5f) setValue (Freeze, 0.0f);
    engine.requestStateReset();
}

void PluginProcessor::sendKeyboardMidi (uint8_t status, uint8_t d1, uint8_t d2) noexcept
{
    const auto scope = kbFifo.write (1);
    MidiEvent e; e.status = status; e.d1 = d1; e.d2 = d2; e.origin = Origin::Keyboard; e.offset = 0;
    if (scope.blockSize1 > 0) kbEvents[(size_t) scope.startIndex1] = e;
    else if (scope.blockSize2 > 0) kbEvents[(size_t) scope.startIndex2] = e;
}

void PluginProcessor::storeSnapshot (int slot)
{
    if (slot < 0 || slot >= 8) return;
    if ((int) snapshots.size() < 8) snapshots.resize (8);
    snapshots[(size_t) slot] = { (int) value (ChRoot), (int) value (ChOctave), (int) value (ChQuality), (int) value (ChInversion), (int) value (ChSpread) };
}

void PluginProcessor::recallSnapshot (int slot)
{
    if (slot < 0 || slot >= (int) snapshots.size()) return;
    const auto& s = snapshots[(size_t) slot];
    setValue (ChRoot, (float) s.root); setValue (ChOctave, (float) s.octave); setValue (ChQuality, (float) s.quality);
    setValue (ChInversion, (float) s.inversion); setValue (ChSpread, (float) s.spread);
}

void PluginProcessor::timerCallback()
{
    const int lat = desiredLatency.load();
    if (lat != getLatencySamples()) setLatencySamples (lat);
    const int snap = engine.telemetry.snapshotRequest.exchange (-1);
    if (snap >= 0) recallSnapshot (snap);
    // rolling wet RMS (A/B loudness match)
    const float e = engine.telemetry.wetEnergy.exchange (0.0f);
    const int c = engine.telemetry.wetEnergyCount.exchange (0);
    if (c > 0)
    {
        wetSumSq = wetSumSq * 0.97 + (double) e;
        wetCount = (int) (wetCount * 0.97) + c;
        if (wetCount > 0) wetRmsDbValue.store ((float) juce::Decibels::gainToDecibels (std::sqrt (wetSumSq / (2.0 * wetCount)), -120.0));
    }
    if (sourcePlayer) sourcePlayer->messageThreadHousekeeping();
}

// ---------------------------------------------------------------------------------------------- audio
void PluginProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { processAny (b, m); }
void PluginProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer& m) { processAny (b, m); }

template <typename T>
void PluginProcessor::processAny (juce::AudioBuffer<T>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();
    const int n = buffer.getNumSamples();
    if (n <= 0) return;

    // transport
    TransportInfo tp;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            tp.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) { tp.bpm = *bpm; tp.hasHost = ! isStandalone(); }
            if (auto ppq = pos->getPpqPosition()) tp.ppqAtBlockStart = *ppq;
        }
    lastHostPlaying.store (tp.playing);

    // latency-changing settings: defer in hosted Studio mode while the transport runs
    const int reqQ = juce::jlimit (0, 2, (int) raw[(size_t) Quality]->load());
    const int reqT = (int) raw[(size_t) Timing]->load();
    const bool affectsHost = ! isStandalone() && (reqT == 1 || appliedTiming == 1);
    if ((reqQ != appliedQuality || reqT != appliedTiming) && affectsHost && tp.playing) pendingLatency.store (true);
    else
    {
        appliedQuality = reqQ; appliedTiming = reqT;
        pendingLatency.store (false);
    }
    desiredLatency.store (appliedTiming == 1 ? engine.studioLatency (appliedQuality) : 0);

    // collect MIDI: host buffer first, then on-screen keyboard
    int nev = 0;
    const int maxEv = (int) events.size();
    if (acceptsMidi())
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (msg.getRawDataSize() > 3 || msg.isSysEx()) continue;
            if (nev >= maxEv) break;
            MidiEvent e;
            e.offset = juce::jlimit (0, n - 1, meta.samplePosition);
            const auto* d = msg.getRawData();
            e.status = d[0];
            e.d1 = msg.getRawDataSize() > 1 ? d[1] : 0;
            e.d2 = msg.getRawDataSize() > 2 ? d[2] : 0;
            if (e.status == kSeqStatus) continue; // reserved internal command byte
            e.origin = Origin::Host;
            events[(size_t) nev++] = e;
        }
    {
        const auto scope = kbFifo.read (kbFifo.getNumReady());
        for (int k = 0; k < scope.blockSize1 && nev < maxEv; ++k) events[(size_t) nev++] = kbEvents[(size_t) (scope.startIndex1 + k)];
        for (int k = 0; k < scope.blockSize2 && nev < maxEv; ++k) events[(size_t) nev++] = kbEvents[(size_t) (scope.startIndex2 + k)];
    }
    // keep events sorted by offset (allocation-free stable insertion sort; host order preserved for ties)
    for (int i = 1; i < nev; ++i)
    {
        const MidiEvent e = events[(size_t) i];
        int j = i - 1;
        while (j >= 0 && events[(size_t) j].offset > e.offset) { events[(size_t) j + 1] = events[(size_t) j]; --j; }
        events[(size_t) j + 1] = e;
    }

    const int numIn = getTotalNumInputChannels();
    const int mainIn = getMainBusNumInputChannels();
    const int inSel = (int) raw[(size_t) InputSource]->load();
    const int slice = (int) inL.size();
    int eventStart = 0;
    for (int pos = 0; pos < n; pos += slice)
    {
        const int len = std::min (slice, n - pos);
        // build input
        for (int i = 0; i < len; ++i) { inL[(size_t) i] = 0.0f; inR[(size_t) i] = 0.0f; }
        auto addCh = [&] (int ch, float* dst) {
            if (ch < numIn) { const T* s = buffer.getReadPointer (ch, pos); for (int i = 0; i < len; ++i) dst[i] += (float) s[i]; }
        };
        const bool useMain = inSel != 2, useSide = inSel != 1;
        const bool allowDeviceInput = ! isStandalone() || liveInput.load();
        if (useMain && allowDeviceInput)
        {
            if (mainIn >= 2) { addCh (0, inL.data()); addCh (1, inR.data()); }
            else if (mainIn == 1) { addCh (0, inL.data()); addCh (0, inR.data()); }
        }
        if (useSide && numIn > mainIn)
        {
            const int sc = numIn - mainIn;
            if (sc >= 2) { addCh (mainIn, inL.data()); addCh (mainIn + 1, inR.data()); }
            else { addCh (mainIn, inL.data()); addCh (mainIn, inR.data()); }
        }

        // events for this slice
        int sliceEvStart = eventStart, sliceEvEnd = eventStart;
        while (sliceEvEnd < nev && events[(size_t) sliceEvEnd].offset < pos + len) ++sliceEvEnd;
        for (int k = sliceEvStart; k < sliceEvEnd; ++k) events[(size_t) k].offset -= pos;
        int sliceCount = sliceEvEnd - sliceEvStart;

        // standalone source (demo / file / experiment) mixes into the input and may add sequence events
        if (sourcePlayer)
        {
            bool wantTailKill = false;
            int extra = 0;
            MidiEvent* extraDst = events.data() + nev;
            sourcePlayer->render (srcL.data(), srcR.data(), len, extraDst, extra, maxEv - nev, wantTailKill);
            for (int i = 0; i < len; ++i) { inL[(size_t) i] += srcL[(size_t) i]; inR[(size_t) i] += srcR[(size_t) i]; }
            if (wantTailKill) engine.requestTailKill();
            sourcePlayer->captureMidi (events.data() + sliceEvStart, sliceCount);
            if (extra > 0)
            {
                // merge sequence events into the slice list (both sorted) without allocating
                std::rotate (events.begin() + sliceEvEnd, events.begin() + nev, events.begin() + nev + extra);
                for (int i = sliceEvEnd; i < sliceEvEnd + extra; ++i)
                {
                    const MidiEvent e = events[(size_t) i];
                    int j = i - 1;
                    while (j >= sliceEvStart && events[(size_t) j].offset > e.offset) { events[(size_t) j + 1] = events[(size_t) j]; --j; }
                    events[(size_t) j + 1] = e;
                }
                sliceCount += extra;
                nev += extra;
                sliceEvEnd += extra;
            }
        }

        TransportInfo stp = tp;
        if (stp.ppqAtBlockStart >= 0.0) stp.ppqAtBlockStart += pos * (tp.bpm / 60.0) / preparedRate;
        processSlice (inL.data(), inR.data(), outL.data(), outR.data(), len, events.data() + sliceEvStart, sliceCount, stp);
        eventStart = sliceEvEnd;

        const int numOut = getTotalNumOutputChannels();
        if (numOut >= 2)
        {
            T* l = buffer.getWritePointer (0, pos); T* r = buffer.getWritePointer (1, pos);
            for (int i = 0; i < len; ++i) { l[i] = (T) outL[(size_t) i]; r[i] = (T) outR[(size_t) i]; }
        }
        else if (numOut == 1)
        {
            T* l = buffer.getWritePointer (0, pos);
            for (int i = 0; i < len; ++i) l[i] = (T) (0.5f * (outL[(size_t) i] + outR[(size_t) i]));
        }
        for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, pos, len);
    }
    midi.clear();

    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const double deadline = n / preparedRate;
    const int w = cpuWrite.load (std::memory_order_relaxed);
    cpuRing[(size_t) w].store ((float) (secs / deadline), std::memory_order_relaxed);
    cpuWrite.store ((w + 1) % kCpuRing, std::memory_order_relaxed);
}

void PluginProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { bypassAny (b, m); }
void PluginProcessor::processBlockBypassed (juce::AudioBuffer<double>& b, juce::MidiBuffer& m) { bypassAny (b, m); }

template <typename T>
void PluginProcessor::bypassAny (juce::AudioBuffer<T>& buffer, juce::MidiBuffer& midi)
{
    midi.clear();
    const int n = buffer.getNumSamples();
    const int lat = getLatencySamples();
    const int mainIn = getMainBusNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int size = (int) bypassLine[0].size();
    if (size == 0 || lat <= 0 || lat >= size)
    {
        for (int ch = mainIn; ch < numOut; ++ch) buffer.clear (ch, 0, n);
        if (mainIn == 1 && numOut >= 2) buffer.copyFrom (1, 0, buffer, 0, 0, n);
        return;
    }
    for (int i = 0; i < n; ++i)
    {
        const int r = (bypassWrite - lat + size) % size;
        for (int ch = 0; ch < 2; ++ch)
        {
            const int src = std::min (ch, std::max (0, mainIn - 1));
            const float x = mainIn > 0 ? (float) buffer.getSample (src, i) : 0.0f;
            bypassLine[ch][(size_t) bypassWrite] = x;
            if (ch < numOut) buffer.setSample (ch, i, (T) bypassLine[ch][(size_t) r]);
        }
        bypassWrite = (bypassWrite + 1) % size;
    }
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
}

void PluginProcessor::processSlice (const float* l, const float* r, float* ol, float* orr, int n, const MidiEvent* ev, int nev, const TransportInfo& tp)
{
    ParamSet ps;
    for (int i = 0; i < kNumParams; ++i) ps[i] = raw[(size_t) i]->load (std::memory_order_relaxed);
    ps[Quality] = (float) appliedQuality;
    ps[Timing] = (float) appliedTiming;
    engine.process (ps, tp, l, r, ol, orr, n, ev, nev);
}

PluginProcessor::CpuStats PluginProcessor::cpuStats() const
{
    std::array<float, kCpuRing> v {};
    int count = 0;
    for (int i = 0; i < kCpuRing; ++i) { const float x = cpuRing[(size_t) i].load (std::memory_order_relaxed); if (x > 0.0f) v[(size_t) count++] = x; }
    CpuStats s;
    s.count = count;
    if (count == 0) return s;
    std::sort (v.begin(), v.begin() + count);
    double sum = 0; for (int i = 0; i < count; ++i) sum += v[(size_t) i];
    s.avg = sum / count;
    s.p95 = v[(size_t) std::min (count - 1, (int) (count * 0.95))];
    s.p99 = v[(size_t) std::min (count - 1, (int) (count * 0.99))];
    s.max = v[(size_t) count - 1];
    return s;
}

juce::String PluginProcessor::buildArchitecture() const
{
#if defined(__aarch64__) || defined(__arm64__)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#else
    return "unknown";
#endif
}

juce::var PluginProcessor::diagnostics() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("product", "Playable Ambience");
    o->setProperty ("version", PA_BUILD_VERSION);
    o->setProperty ("git", PA_BUILD_GIT);
    o->setProperty ("juce", juce::String (PA_JUCE_TAG) + " (" + PA_JUCE_COMMIT + ")");
    o->setProperty ("architecture", buildArchitecture());
    o->setProperty ("wrapper", juce::AudioProcessor::getWrapperTypeDescription (wrapperType));
    o->setProperty ("companionAudioAU", isCompanion());
    o->setProperty ("os", juce::SystemStats::getOperatingSystemName());
    o->setProperty ("sampleRate", preparedRate);
    o->setProperty ("blockSize", preparedBlock);
    o->setProperty ("reportedLatencySamples", getLatencySamples());
    o->setProperty ("wetProcessingLatencySamples", engine.telemetry.wetLatency.load());
    o->setProperty ("timing", appliedTiming == 1 ? "Studio" : "Live");
    o->setProperty ("quality", juce::StringArray { "Eco", "Standard", "High" }[appliedQuality]);
    o->setProperty ("harmonyMethod", effectiveHarmonyMethod (currentParams()) == 1 ? "Classic" : "Off");
    o->setProperty ("dryWetPercent", value (Mix));
    o->setProperty ("noteSource", juce::String (paramValueToText (NoteSource, value (NoteSource))));
    o->setProperty ("routing", juce::String (paramValueToText (Routing, value (Routing))));
    o->setProperty ("placement", juce::String (paramValueToText (Placement, value (Placement))));
    o->setProperty ("midiEventsReceived", engine.telemetry.midiCounter.load());
    o->setProperty ("protectionLimiterEvents", engine.telemetry.protectionCount.load());
    o->setProperty ("nonFiniteResets", engine.telemetry.nonfiniteCount.load());
    const auto c = cpuStats();
    auto* cpu = new juce::DynamicObject();
    cpu->setProperty ("avgDeadlineFraction", c.avg); cpu->setProperty ("p95", c.p95); cpu->setProperty ("p99", c.p99);
    cpu->setProperty ("max", c.max); cpu->setProperty ("blocks", c.count);
    o->setProperty ("cpu", juce::var (cpu));
    juce::Array<juce::var> voices;
    for (int v = 0; v < 6; ++v)
    {
        const int note = engine.telemetry.voiceNote[(size_t) v].load();
        if (note >= 0) voices.add (juce::String (paramValueToText (ShRef, (float) note)) + (engine.telemetry.voiceGate[(size_t) v].load() ? " (held)" : " (releasing)"));
    }
    o->setProperty ("voicedNotes", voices);
    return juce::var (o);
}

// ---------------------------------------------------------------------------------------------- state
std::unique_ptr<juce::XmlElement> PluginProcessor::stateToXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("PlayableAmbience");
    xml->setAttribute ("schema", 1);
    xml->setAttribute ("version", PA_BUILD_VERSION);
    auto* pe = xml->createNewChildElement ("PARAMS");
    for (int i = 0; i < kNumParams; ++i)
    {
        auto* p = pe->createNewChildElement ("P");
        p->setAttribute ("id", paramInfo (i).id);
        p->setAttribute ("v", (double) raw[(size_t) i]->load());
    }
    auto* se = xml->createNewChildElement ("SNAPSHOTS");
    for (const auto& s : snapshots)
    {
        auto* x = se->createNewChildElement ("S");
        x->setAttribute ("root", s.root); x->setAttribute ("octave", s.octave); x->setAttribute ("quality", s.quality);
        x->setAttribute ("inversion", s.inversion); x->setAttribute ("spread", s.spread);
    }
    auto* meta = xml->createNewChildElement ("META");
    meta->setAttribute ("preset", presetManager->currentName());
    meta->setAttribute ("advancedTab", uiAdvancedTab);
    meta->setAttribute ("clearTailOnAB", clearTailOnAB);
    if (auto ab = presetManager->abToXml()) xml->addChildElement (ab.release());
    return xml;
}

void PluginProcessor::stateFromXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName ("PlayableAmbience")) return;
    ParamSet ps; // defaults for anything missing
    if (auto* pe = xml.getChildByName ("PARAMS"))
        for (auto* p : pe->getChildIterator())
        {
            const int idx = paramIndexForId (p->getStringAttribute ("id").toStdString());
            if (idx >= 0) ps[idx] = paramSnap (idx, (float) p->getDoubleAttribute ("v", paramInfo (idx).def));
        }
    migrateLegacyHarmony (ps); // sessions saved before the Classic-only build
    for (int i = 0; i < kNumParams; ++i)
        if (auto* p = params[(size_t) i]) p->setValueNotifyingHost (p->convertTo0to1 (ps[i]));
    if (auto* se = xml.getChildByName ("SNAPSHOTS"))
    {
        snapshots.clear();
        for (auto* x : se->getChildIterator())
            snapshots.push_back ({ x->getIntAttribute ("root"), x->getIntAttribute ("octave", 3), x->getIntAttribute ("quality", 1),
                                   x->getIntAttribute ("inversion"), x->getIntAttribute ("spread") });
        while (snapshots.size() < 8) snapshots.push_back ({});
    }
    if (auto* meta = xml.getChildByName ("META"))
    {
        presetManager->setCurrentName (meta->getStringAttribute ("preset", "Untitled"));
        uiAdvancedTab = meta->getStringAttribute ("advancedTab", "Harmony");
        clearTailOnAB = meta->getBoolAttribute ("clearTailOnAB", false);
    }
    if (auto* ab = xml.getChildByName ("AB")) presetManager->abFromXml (*ab);
    engine.requestStateReset();
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = stateToXml()) copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes)) stateFromXml (*xml);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor (*this); }

} // namespace pa

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new pa::PluginProcessor(); }
