#include "SourcePlayer.h"

namespace pa
{
SourcePlayer::SourcePlayer() : pool (std::make_unique<juce::ThreadPool> (1)) {}

SourcePlayer::~SourcePlayer()
{
    pool->removeAllJobs (true, 4000);
    delete pending.exchange (nullptr);
    delete retired.exchange (nullptr);
    delete active;
}

void SourcePlayer::prepare (double sampleRate, int)
{
    const bool changed = std::abs (sampleRate - sr) > 0.5 || active == nullptr;
    sr = sampleRate;
    if (! changed) return;
    // (Re)build the current source at the new device rate. Called outside the audio callback.
    playing.store (false);
    pos = 0; posAtomic.store (0);
    auto cur = lastSubmitted;
    std::unique_ptr<Data> d;
    if (cur && cur->kind == Kind::Experiment) { selectExperiment (cur->index); return; }
    if (cur && cur->kind == Kind::File && std::abs (cur->sampleRate - sr) < 0.5)
    {
        d = std::make_unique<Data> (*cur);
    }
    else d = makeDemo (cur && cur->kind == Kind::Demo ? cur->index : (int) Fixture::HarmonicTone);
    delete active;
    active = d.release();
    lastSubmitted = std::make_shared<Data> (*active);
}

std::unique_ptr<SourcePlayer::Data> SourcePlayer::makeDemo (int fixture) const
{
    auto d = std::make_unique<Data>();
    std::vector<float> l, r;
    const auto f = (Fixture) juce::jlimit (0, (int) Fixture::Count - 1, fixture);
    generateFixture (f, sr, l, r);
    d->audio.setSize (2, (int) l.size());
    d->audio.copyFrom (0, 0, l.data(), (int) l.size());
    d->audio.copyFrom (1, 0, r.data(), (int) r.size());
    d->sampleRate = sr;
    d->name = fixtureInfo (f).name;
    d->info = fixtureInfo (f).description;
    d->kind = Kind::Demo;
    d->index = (int) f;
    return d;
}

void SourcePlayer::submit (std::unique_ptr<Data> d)
{
    lastSubmitted = std::make_shared<Data> (*d);
    delete pending.exchange (d.release());
}

void SourcePlayer::selectDemo (int fixture)
{
    submit (makeDemo (fixture));
    status = lastSubmitted->name;
}

void SourcePlayer::selectExperiment (int index)
{
    const auto& ex = builtInExperiments();
    if (index < 0 || index >= (int) ex.size()) return;
    const auto& e = ex[(size_t) index];
    auto d = makeDemo ((int) e.fixture);
    // pad the source with silence up to the experiment duration so the sequence runs over the tail
    const int total = (int) (e.duration * sr);
    if (d->audio.getNumSamples() < total)
    {
        juce::AudioBuffer<float> b (2, total);
        b.clear();
        for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, d->audio, ch, 0, d->audio.getNumSamples());
        d->audio = std::move (b);
    }
    d->events = e.events;
    d->markers = e.markers;
    d->name = juce::String ("Experiment: ") + e.name;
    d->info = e.description;
    d->kind = Kind::Experiment;
    d->index = index;
    submit (std::move (d));
    status = lastSubmitted->name;
}

bool SourcePlayer::loadFile (const juce::File& f, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    if (fm.findFormatForFileExtension (f.getFileExtension()) == nullptr) { error = "Unsupported file type (use WAV, AIFF or FLAC)."; return false; }
    const int gen = ++loadGeneration;
    loadingMessage = "Loading " + f.getFileName() + "...";
    const double targetSr = sr;
    pool->addJob ([this, f, gen, targetSr] {
        juce::AudioFormatManager fmt;
        fmt.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fmt.createReaderFor (f));
        juce::String msg;
        if (reader == nullptr) msg = "Could not read " + f.getFileName();
        else
        {
            const double srcSr = reader->sampleRate > 0 ? reader->sampleRate : 48000.0;
            const auto maxSrc = (juce::int64) (kMaxFileSeconds * srcSr);
            const auto len = std::min (reader->lengthInSamples, maxSrc);
            juce::AudioBuffer<float> raw (2, (int) len);
            reader->read (&raw, 0, (int) len, 0, true, reader->numChannels > 1);
            if (reader->numChannels == 1) raw.copyFrom (1, 0, raw, 0, 0, (int) len);
            auto d = std::make_unique<Data>();
            const double ratio = srcSr / targetSr;
            const int outLen = (int) std::ceil (len / ratio);
            d->audio.setSize (2, outLen);
            for (int ch = 0; ch < 2; ++ch)
            {
                juce::LagrangeInterpolator interp;
                interp.process (ratio, raw.getReadPointer (ch), d->audio.getWritePointer (ch), outLen, (int) len, 0);
            }
            d->sampleRate = targetSr;
            d->name = f.getFileName();
            d->info = juce::String (srcSr, 0) + " Hz source, " + juce::String (len / srcSr, 1) + " s"
                    + (reader->lengthInSamples > maxSrc ? " (truncated to the 2-minute maximum)" : "");
            d->kind = Kind::File;
            if (gen == loadGeneration.load())
            {
                juce::MessageManager::callAsync ([this, dd = d.release()] () mutable {
                    submit (std::unique_ptr<Data> (dd));
                    status = lastSubmitted->name + " - " + lastSubmitted->info;
                    loadingMessage = {};
                });
                return;
            }
        }
        juce::MessageManager::callAsync ([this, msg] { loadingMessage = msg; });
    });
    return true;
}

void SourcePlayer::play() { cmd.store (1); }
void SourcePlayer::pause() { cmd.store (2); }
void SourcePlayer::stop() { cmd.store (3); }
void SourcePlayer::seek (double seconds) { seekTo.store ((long long) std::max (0.0, seconds * sr)); }

double SourcePlayer::positionSeconds() const noexcept { return (double) posAtomic.load() / sr; }
double SourcePlayer::durationSeconds() const noexcept
{
    return lastSubmitted ? lastSubmitted->audio.getNumSamples() / std::max (1.0, lastSubmitted->sampleRate) : 0.0;
}

void SourcePlayer::messageThreadHousekeeping()
{
    delete retired.exchange (nullptr);
}

void SourcePlayer::setRecording (bool b)
{
    if (b) { recCount.store (0); recStartPos.store (posAtomic.load()); }
    recording.store (b);
}

std::vector<TimedEvent> SourcePlayer::recordedEvents() const
{
    std::vector<TimedEvent> out;
    const int n = recCount.load();
    for (int i = 0; i < n; ++i) out.push_back (rec[(size_t) i]);
    return out;
}

void SourcePlayer::captureMidi (const MidiEvent* ev, int n) noexcept
{
    if (! recording.load (std::memory_order_relaxed)) return;
    for (int i = 0; i < n; ++i)
    {
        const int c = recCount.load (std::memory_order_relaxed);
        if (c >= kRecMax) return;
        const int type = ev[i].status & 0xF0;
        if (type != 0x80 && type != 0x90 && type != 0xB0 && type != 0xE0) continue;
        rec[(size_t) c] = { (double) (pos + ev[i].offset - recStartPos.load (std::memory_order_relaxed)) / sr, ev[i].status, ev[i].d1, ev[i].d2 };
        recCount.store (c + 1, std::memory_order_release);
    }
}

void SourcePlayer::render (float* L, float* R, int n, MidiEvent* outEvents, int& numOut, int maxOut, bool& requestTailKill) noexcept
{
    numOut = 0;
    requestTailKill = false;
    for (int i = 0; i < n; ++i) { L[i] = 0.0f; R[i] = 0.0f; }

    // adopt newly prepared data
    if (Data* np = pending.load (std::memory_order_acquire))
    {
        if (retired.load (std::memory_order_acquire) == nullptr && pending.compare_exchange_strong (np, nullptr))
        {
            retired.store (active, std::memory_order_release);
            active = np;
            pos = 0; fade = 0.0f;
            if (numOut < maxOut) { outEvents[numOut++] = { 0, 0xB0, 123, 0, Origin::Sequence }; }
            if (numOut < maxOut) { outEvents[numOut++] = { 0, kSeqStatus, SeqFreezeOff, 0, Origin::Sequence }; }
        }
    }
    const int c = cmd.exchange (0);
    if (c == 1) { if (active && pos >= active->audio.getNumSamples()) pos = 0; playing.store (true); }
    else if (c == 2) playing.store (false);
    else if (c == 3)
    {
        playing.store (false);
        pos = 0;
        if (numOut < maxOut) outEvents[numOut++] = { 0, 0xB0, 123, 0, Origin::Sequence };
        if (numOut < maxOut) outEvents[numOut++] = { 0, kSeqStatus, SeqFreezeOff, 0, Origin::Sequence };
        if (! keepTail.load()) requestTailKill = true;
    }
    const long long sk = seekTo.exchange (-1);
    if (sk >= 0 && active)
    {
        pos = std::min<long long> (sk, active->audio.getNumSamples());
        fade = 0.0f;
        if (! active->events.empty() && numOut < maxOut) outEvents[numOut++] = { 0, 0xB0, 123, 0, Origin::Sequence };
    }
    if (! active || ! playing.load()) { posAtomic.store (pos); wasPlaying = false; return; }

    const int len = active->audio.getNumSamples();
    if (len <= 0) return;
    const float* a = active->audio.getReadPointer (0);
    const float* b = active->audio.getReadPointer (1);
    const float g = level.load();
    const float fadeStep = 1.0f / (0.01f * (float) sr);
    const int fadeLen = (int) (0.01 * sr);
    const bool hasEvents = ! active->events.empty();
    for (int i = 0; i < n; ++i)
    {
        if (pos >= len)
        {
            if (loop.load())
            {
                pos = 0; fade = 0.0f;
                if (hasEvents)
                {
                    if (numOut < maxOut) outEvents[numOut++] = { i, 0xB0, 123, 0, Origin::Sequence };
                    if (numOut < maxOut) outEvents[numOut++] = { i, kSeqStatus, SeqFreezeOff, 0, Origin::Sequence };
                }
            }
            else
            {
                playing.store (false);
                if (hasEvents && numOut < maxOut) outEvents[numOut++] = { i, 0xB0, 123, 0, Origin::Sequence };
                break;
            }
        }
        if (hasEvents)
        {
            // sequence events scheduled at this sample
            const double t0 = (double) pos / sr, t1 = (double) (pos + 1) / sr;
            for (const auto& e : active->events)
            {
                if (e.time < t0) continue;
                if (e.time >= t1) break;
                if (numOut < maxOut) outEvents[numOut++] = { i, e.status, e.d1, e.d2, Origin::Sequence };
            }
        }
        fade = std::min (1.0f, fade + fadeStep);
        float env = fade;
        if (loop.load() && len - pos < fadeLen) env = std::min (env, (float) (len - pos) / (float) fadeLen);
        L[i] = a[pos] * g * env;
        R[i] = b[pos] * g * env;
        ++pos;
    }
    posAtomic.store (pos);
}
} // namespace pa
