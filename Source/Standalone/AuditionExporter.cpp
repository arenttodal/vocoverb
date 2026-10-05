#include "AuditionExporter.h"

namespace pa
{
bool AuditionExporter::start (Job&& j, juce::String& error)
{
    if (isThreadRunning()) { error = "An export is already running."; return false; }
    j.tailSeconds = juce::jlimit (0.0, kMaxTailSeconds, j.tailSeconds);
    const double total = j.source.getNumSamples() / j.sampleRate + j.tailSeconds;
    if (total > kMaxTotalSeconds) { error = "Export too long (max " + juce::String (kMaxTotalSeconds, 0) + " s)."; return false; }
    if (j.output == juce::File()) { error = "No output file."; return false; }
    job = std::move (j);
    cancelled.store (false);
    prog.store (0.0f);
    startThread();
    return true;
}

bool AuditionExporter::renderToBuffer (const Job& job, juce::AudioBuffer<float>& out, std::atomic<bool>* cancel, std::atomic<float>* progress)
{
    const double sr = job.sampleRate;
    const int srcLen = job.source.getNumSamples();
    const int total = srcLen + (int) (job.tailSeconds * sr);
    out.setSize (2, total);
    out.clear();
    auto engine = std::make_unique<Engine>();
    constexpr int block = 512;
    engine->prepare (sr, block);
    ParamSet p = job.params;
    p[Timing] = 0; // offline: dry immediate, wet carries its natural processing delay
    std::vector<MidiEvent> ev (1024);
    std::vector<float> inL (block), inR (block);
    TransportInfo tp;
    for (int pos = 0; pos < total; pos += block)
    {
        if (cancel && cancel->load()) return false;
        const int n = std::min (block, total - pos);
        for (int i = 0; i < n; ++i)
        {
            const int s = pos + i;
            inL[(size_t) i] = s < srcLen ? job.source.getSample (0, s) : 0.0f;
            inR[(size_t) i] = s < srcLen ? job.source.getSample (job.source.getNumChannels() > 1 ? 1 : 0, s) : 0.0f;
        }
        const int ne = eventsInWindow (job.events, pos / sr, (pos + n) / sr, sr, ev.data(), (int) ev.size(), Origin::Sequence);
        engine->process (p, tp, inL.data(), inR.data(), out.getWritePointer (0, pos), out.getWritePointer (1, pos), n, ev.data(), ne);
        if (progress) progress->store ((float) (pos + n) / (float) total);
    }
    return true;
}

void AuditionExporter::run()
{
    juce::AudioBuffer<float> out;
    const bool ok = renderToBuffer (job, out, &cancelled, &prog);
    juce::String msg;
    bool success = false;
    if (! ok) msg = "Export cancelled.";
    else
    {
        job.output.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (job.output.createOutputStream().release());
        if (os == nullptr) msg = "Cannot write " + job.output.getFullPathName();
        else
        {
            auto opts = juce::AudioFormatWriterOptions().withSampleRate (job.sampleRate).withNumChannels (2).withBitsPerSample (24);
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os, opts));
            if (w == nullptr) msg = "WAV writer failed";
            else
            {
                w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
                w.reset();
                msg = "Exported " + job.output.getFileName() + " (" + juce::String (out.getNumSamples() / job.sampleRate, 1) + " s).";
                success = true;
            }
        }
    }
    {
        const juce::ScopedLock sl (lock);
        resultText = msg;
    }
    prog.store (1.0f);
    auto cb = onFinished;
    juce::MessageManager::callAsync ([cb, success, msg] { if (cb) cb (success, msg); });
}
} // namespace pa
