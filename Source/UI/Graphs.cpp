#include "Graphs.h"

namespace pa
{
void readHistory (const std::array<std::atomic<float>, Telemetry::kHistLen>& ring, int write, int count, std::vector<float>& out)
{
    count = std::min (count, Telemetry::kHistLen - 1);
    out.resize ((size_t) count);
    for (int i = 0; i < count; ++i)
    {
        const int idx = (write - count + i + Telemetry::kHistLen) & (Telemetry::kHistLen - 1);
        out[(size_t) i] = ring[(size_t) idx].load (std::memory_order_relaxed);
    }
}

namespace
{
float levelToUnit (float lin) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (lin, -72.0f) + 66.0f) / 66.0f); }

void drawTimeAxis (juce::Graphics& g, juce::Rectangle<float> r, double spanSec, int divisions)
{
    g.setFont (theme::font (11.0f));
    g.setColour (theme::displayLabel);
    for (int i = 0; i <= divisions; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) divisions;
        const double t = spanSec * (double) (divisions - i) / divisions;
        juce::String s = i == divisions ? juce::String ("now") : ("-" + juce::String (t, t < 10 ? (std::fmod (t, 1.0) > 0.01 ? 1 : 0) : 0) + " s");
        const int w = 52;
        auto lr = juce::Rectangle<int> ((int) x - (i == 0 ? 2 : (i == divisions ? w - 4 : w / 2)), (int) r.getBottom() - 16, w, 14);
        g.drawText (s, lr, i == 0 ? juce::Justification::centredLeft : (i == divisions ? juce::Justification::centredRight : juce::Justification::centred));
        g.setColour (theme::displayLabel.withAlpha (0.4f));
        g.drawVerticalLine ((int) x, r.getBottom() - 20.0f, r.getBottom() - 17.0f);
        g.setColour (theme::displayLabel);
    }
}

void drawPill (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, juce::Colour fill, juce::Colour fg)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (fg);
    g.setFont (theme::spaced (11.0f, 0.12f, 2));
    g.drawText (text, r, juce::Justification::centred);
}

int niceDivisions (double span)
{
    if (span <= 5.0) return (int) std::ceil (span);
    if (span <= 12.0) return (int) std::ceil (span / 2.0);
    return (int) std::ceil (span / 5.0);
}
} // namespace

// ============================================================================ DelayGraph
void DelayGraph::update()
{
    auto& t = proc.telemetry();
    const double sr = t.sampleRate.load();
    delayMs = t.delayTimeMs.load();
    spanSec = juce::jlimit (2.0, 16.0, std::max (4.5, delayMs * 0.001 * 9.0));
    spanSec = std::ceil (spanSec);
    const int buckets = (int) (spanSec * sr / Telemetry::kBucket);
    readHistory (t.histDelay, t.histWrite.load(), buckets, hist);
    frozen = t.delayFrozen.load();
    const bool interval = proc.value (DelayMode) > 0.5f;
    taps.clear();
    if (interval)
    {
        const int n = (int) proc.value (IvTaps);
        for (int k = 0; k < n; ++k) taps.push_back ((int) proc.value (IvTap1Semi + k));
        legend = juce::String ("INTERVAL  ") + (proc.value (IvPitchMode) > 0.5f ? "CLOCK" : "STABLE") + "  "
                 + paramValueToText (IvDirection, proc.value (IvDirection)) + "  " + paramValueToText (IvShiftPlace, proc.value (IvShiftPlace));
    }
    else legend = juce::String ("BBD  ") + juce::String (delayMs, 0) + " ms  FB " + juce::String ((int) proc.value (BbdFeedback)) + "%";
    repaint();
}

void DelayGraph::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    paintDisplay (g, r, 8, 4);
    auto plot = r.reduced (10.0f, 8.0f).withTrimmedBottom (16.0f);
    const float mid = plot.getCentreY();
    // centre line
    g.setColour (theme::accent.withAlpha (0.45f));
    g.drawHorizontalLine ((int) mid, plot.getX(), plot.getRight());
    const int n = (int) hist.size();
    if (n > 1)
    {
        const float w = plot.getWidth();
        juce::Path top;
        const int cols = (int) w;
        std::vector<float> colv ((size_t) cols, 0.0f);
        for (int x = 0; x < cols; ++x)
        {
            const int a = (int) ((float) x / (float) cols * (float) n), b = std::max (a + 1, (int) ((float) (x + 1) / (float) cols * (float) n));
            float m = 0.0f;
            for (int i = a; i < b && i < n; ++i) m = std::max (m, hist[(size_t) i]);
            colv[(size_t) x] = levelToUnit (m);
        }
        for (int x = 0; x < cols; ++x)
        {
            const float v = colv[(size_t) x];
            if (v <= 0.01f) continue;
            const float h = v * plot.getHeight() * 0.48f;
            const float alpha = 0.25f + 0.75f * v;
            g.setGradientFill (juce::ColourGradient (theme::amber.withAlpha (alpha), plot.getX() + (float) x, mid,
                                                     theme::accent.withAlpha (alpha * 0.5f), plot.getX() + (float) x, mid - h, false));
            g.fillRect (plot.getX() + (float) x, mid - h, 1.0f, h * 2.0f);
        }
        // bright core where energy is high
        for (int x = 0; x < cols; ++x)
            if (colv[(size_t) x] > 0.55f)
            {
                g.setColour (theme::ivoryTrace.withAlpha ((colv[(size_t) x] - 0.55f) * 1.6f));
                g.fillRect (plot.getX() + (float) x, mid - 1.0f, 1.0f, 2.0f);
            }
    }
    // repeat timing markers (multiples of the delay time back from now)
    if (delayMs > 1.0)
    {
        g.setColour (theme::displayLabel.withAlpha (0.55f));
        for (int k = 1; k * delayMs * 0.001 < spanSec && k < 64; ++k)
        {
            const float x = plot.getRight() - plot.getWidth() * (float) (k * delayMs * 0.001 / spanSec);
            g.fillEllipse (x - 1.5f, plot.getBottom() - 4.0f, 3.0f, 3.0f);
        }
    }
    drawTimeAxis (g, r.reduced (10.0f, 2.0f), spanSec, niceDivisions (spanSec));
    // legend & taps
    g.setFont (theme::spaced (11.0f, 0.1f, 1));
    g.setColour (theme::displayLabel);
    g.drawText (legend, plot.toNearestInt().removeFromTop (16).withTrimmedLeft (4), juce::Justification::centredLeft);
    if (! taps.empty())
    {
        float x = plot.getX() + 4.0f;
        for (int s : taps)
        {
            auto pr = juce::Rectangle<float> (x, plot.getY() + 20.0f, 36.0f, 16.0f);
            drawPill (g, pr, (s >= 0 ? "+" : "") + juce::String (s), theme::accent.withAlpha (0.18f), theme::amber);
            x += 40.0f;
        }
    }
    if (frozen) drawPill (g, juce::Rectangle<float> (70.0f, 18.0f).withPosition (plot.getRight() - 74.0f, plot.getY() + 22.0f), "FROZEN", theme::accent, juce::Colours::white);
    if (overlay.isNotEmpty())
    {
        g.setColour (theme::display.withAlpha (0.82f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (theme::ivoryTrace);
        g.setFont (theme::font (14.0f, 1));
        g.drawFittedText (overlay, r.reduced (20.0f).toNearestInt(), juce::Justification::centred, 3);
    }
}

// ============================================================================ ReverbGraph
void ReverbGraph::update()
{
    auto& t = proc.telemetry();
    const double sr = t.sampleRate.load();
    const bool wash = proc.value (ReverbMode) > 0.5f;
    decaySec = wash ? proc.value (WaDecay) : proc.value (PlDecay);
    const double maxSpan = (Telemetry::kHistLen - 2) * (double) Telemetry::kBucket / sr;
    spanSec = std::floor (juce::jlimit (6.0, std::min (20.0, maxSpan), decaySec * (wash ? 0.6 : 1.0)));
    const int buckets = (int) (spanSec * sr / Telemetry::kBucket);
    histEnd = t.histWrite.load();
    readHistory (t.histReverb, histEnd, buckets, hist);
    frozen = t.reverbFrozen.load();
    legend = juce::String (wash ? "WASH  " : "PLATE  ") + "DECAY " + juce::String (decaySec, decaySec < 10 ? 1 : 0) + " s";
    if (decaySec > spanSec) legend << "  (rolling " << (int) spanSec << " s view)";
    repaint();
}

void ReverbGraph::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    paintDisplay (g, r, 8, 4);
    auto plot = r.reduced (10.0f, 8.0f).withTrimmedBottom (16.0f);
    const int n = (int) hist.size();
    if (n > 1)
    {
        const int cols = std::max (1, (int) plot.getWidth() / 2);
        for (int c = 0; c < cols; ++c)
        {
            const int a = c * n / cols, b = std::max (a + 1, (c + 1) * n / cols);
            float m = 0.0f;
            for (int i = a; i < b && i < n; ++i) m = std::max (m, hist[(size_t) i]);
            const float v = levelToUnit (m);
            if (v <= 0.02f) continue;
            const float age = 1.0f - (float) c / (float) cols; // 0 = now, 1 = oldest
            juce::Colour col = theme::accent.interpolatedWith (theme::ivoryTrace, juce::jlimit (0.0f, 1.0f, age * 1.4f))
                                             .interpolatedWith (theme::displayLabel, juce::jlimit (0.0f, 1.0f, age * 1.6f - 0.6f));
            // deterministic particles tied to absolute bucket index so the cloud scrolls with time
            const uint32_t absIdx = (uint32_t) (histEnd - n + b);
            Rng rng (absIdx * 2654435761u + 17u);
            const int particles = (int) (v * v * 26.0f) + 1;
            const float spread = v * plot.getHeight() * 0.47f;
            for (int k = 0; k < particles; ++k)
            {
                const float yy = plot.getCentreY() + rng.bi() * spread * std::sqrt (rng.uni());
                const float xx = plot.getX() + (float) c * 2.0f + rng.uni() * 2.0f;
                const float sz = 0.8f + rng.uni() * 1.4f;
                g.setColour (col.withAlpha (juce::jlimit (0.08f, 0.9f, 0.25f + 0.65f * v * rng.uni())));
                g.fillEllipse (xx, yy, sz, sz);
            }
        }
    }
    drawTimeAxis (g, r.reduced (10.0f, 2.0f), spanSec, niceDivisions (spanSec));
    g.setFont (theme::spaced (11.0f, 0.1f, 1));
    g.setColour (theme::displayLabel);
    g.drawText (legend, plot.toNearestInt().removeFromTop (16).withTrimmedLeft (4), juce::Justification::centredLeft);
    if (frozen) drawPill (g, juce::Rectangle<float> (70.0f, 18.0f).withPosition (plot.getRight() - 74.0f, plot.getY() + 22.0f), "FROZEN", theme::accent, juce::Colours::white);
    if (overlay.isNotEmpty())
    {
        g.setColour (theme::display.withAlpha (0.82f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (theme::ivoryTrace);
        g.setFont (theme::font (14.0f, 1));
        g.drawFittedText (overlay, r.reduced (20.0f).toNearestInt(), juce::Justification::centred, 3);
    }
}

// ============================================================================ HarmonyGraph
HarmonyGraph::HarmonyGraph (PluginProcessor& p) : proc (p)
{
    fft.prepare (kN);
    buf.resize (kN);
    window.resize (kN);
    for (int i = 0; i < kN; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * kPi * (float) i / (float) (kN - 1));
    wetHist.assign (kHistPoints, 0.0f);
}

float HarmonyGraph::measureNote (int note) const
{
    const float a4 = proc.value (RefTuning);
    const float f0 = midiToHz ((float) note, a4);
    float e = 0.0f;
    const float wts[3] = { 1.0f, 0.55f, 0.35f };
    for (int h = 1; h <= 3; ++h)
    {
        const float f = f0 * (float) h;
        const int lo = std::max (1, (int) std::floor (f * std::exp2 (-40.0f / 1200.0f) / binHz));
        const int hi = std::min ((int) mag.size() - 1, (int) std::ceil (f * std::exp2 (40.0f / 1200.0f) / binHz));
        float m = 0.0f;
        for (int k = lo; k <= hi; ++k) m = std::max (m, mag[(size_t) k]);
        e += wts[h - 1] * m;
    }
    return e;
}

void HarmonyGraph::update()
{
    auto& t = proc.telemetry();
    const double sr = t.sampleRate.load();
    binHz = sr / kN;
    now += 1.0 / 25.0;
    // latest wet samples -> spectrum (UI thread only)
    const int w = t.specWrite.load (std::memory_order_relaxed);
    float sumSq = 0.0f;
    for (int i = 0; i < kN; ++i)
    {
        const float v = t.spec[(size_t) ((w - kN + i) & (Telemetry::kSpecLen - 1))].load (std::memory_order_relaxed);
        sumSq += v * v;
        buf[(size_t) i] = { v * window[(size_t) i], 0.0f };
    }
    fft.forward (buf.data(), kN);
    mag.resize (kN / 2 + 1);
    for (int k = 0; k <= kN / 2; ++k) mag[(size_t) k] = std::abs (buf[(size_t) k]) * (2.0f / (float) kN * 2.0f);
    const float wetRms = std::sqrt (sumSq / kN);
    std::rotate (wetHist.begin(), wetHist.begin() + 1, wetHist.end());
    wetHist.back() = levelToUnit (wetRms * 1.4f);

    // voiced notes
    std::vector<int> activeNotes;
    for (int v = 0; v < 6; ++v)
    {
        const int note = t.voiceNote[(size_t) v].load();
        if (note < 0 || t.voiceEnv[(size_t) v].load() < 1.0e-4f) continue;
        auto& tr = tracks[note];
        if (tr.level.empty()) tr.level.assign (kHistPoints, 0.0f);
        const bool gated = t.voiceGate[(size_t) v].load();
        tr.active = gated;
        if (gated) { tr.lastActive = now; activeNotes.push_back (note); }
    }
    for (auto& [note, tr] : tracks)
    {
        bool voiced = false;
        for (int v = 0; v < 6; ++v) if (t.voiceNote[(size_t) v].load() == note && t.voiceGate[(size_t) v].load()) voiced = true;
        tr.active = voiced;
        const float e = measureNote (note);
        std::rotate (tr.level.begin(), tr.level.begin() + 1, tr.level.end());
        tr.level.back() = levelToUnit (e);
    }
    // forget tracks that have been released and silent for the whole history
    for (auto it = tracks.begin(); it != tracks.end();)
    {
        if (! it->second.active && now - it->second.lastActive > 8.0) it = tracks.erase (it);
        else ++it;
    }
    std::sort (activeNotes.begin(), activeNotes.end());
    activeNotes.erase (std::unique (activeNotes.begin(), activeNotes.end()), activeNotes.end());
    const bool holding = t.holdingLast.load();
    chordText = activeNotes.empty() ? juce::String ("NO NOTES") : chordName (activeNotes);
    if (holding && ! activeNotes.empty()) chordText = "HOLD  " + chordText;
    const int src = (int) proc.value (NoteSource);
    statusText = juce::String (paramValueToText (NoteSource, (float) src)).toUpperCase() + "  /  "
               + juce::String (paramValueToText (NoNotePolicy, proc.value (NoNotePolicy))).toUpperCase()
               + (proc.value (Latch) > 0.5f ? "  /  LATCH" : "")
               + "  /  " + juce::String (paramValueToText (HarmMethod, proc.value (HarmMethod))).toUpperCase();
    repaint();
}

void HarmonyGraph::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    paintDisplay (g, r, 8, 3);
    auto plot = r.reduced (12.0f, 10.0f).withTrimmedBottom (14.0f).withTrimmedLeft (26.0f);
    // ordinary wet energy (pale ivory, bottom)
    {
        juce::Path p;
        p.startNewSubPath (plot.getX(), plot.getBottom());
        for (int i = 0; i < kHistPoints; ++i)
            p.lineTo (plot.getX() + plot.getWidth() * (float) i / (float) (kHistPoints - 1), plot.getBottom() - wetHist[(size_t) i] * plot.getHeight() * 0.22f);
        p.lineTo (plot.getRight(), plot.getBottom());
        p.closeSubPath();
        g.setColour (theme::ivoryTrace.withAlpha (0.07f));
        g.fillPath (p);
    }
    // note rows (high pitch on top)
    std::vector<int> notes;
    for (auto& [n, tr] : tracks) notes.push_back (n);
    std::sort (notes.rbegin(), notes.rend());
    const int rows = (int) notes.size();
    const bool sharps = keyPrefersSharps ((int) proc.value (ChRoot));
    for (int i = 0; i < rows; ++i)
    {
        const auto& tr = tracks[notes[(size_t) i]];
        const float cy = plot.getY() + plot.getHeight() * ((float) i + 0.5f) / (float) std::max (rows, 3) + (rows < 3 ? plot.getHeight() * (3 - rows) / 6.0f : 0.0f);
        const juce::Colour col = tr.active ? theme::accent : theme::amber.withAlpha (0.55f);
        juce::Path band;
        const int np = (int) tr.level.size();
        for (int k = 0; k < np; ++k)
        {
            const float x = plot.getX() + plot.getWidth() * (float) k / (float) (np - 1);
            const float v = tr.level[(size_t) k];
            const float wiggle = (k > 0 ? (tr.level[(size_t) k] - tr.level[(size_t) k - 1]) : 0.0f) * 40.0f;
            const float y = cy - juce::jlimit (-6.0f, 6.0f, wiggle) - v * 3.0f;
            if (k == 0) band.startNewSubPath (x, y); else band.lineTo (x, y);
        }
        // glow width follows the measured level at each point (draw as segments)
        for (int k = 1; k < np; ++k)
        {
            const float v = tr.level[(size_t) k];
            if (v < 0.03f) continue;
            const float x0 = plot.getX() + plot.getWidth() * (float) (k - 1) / (float) (np - 1);
            const float x1 = plot.getX() + plot.getWidth() * (float) k / (float) (np - 1);
            g.setColour (col.withAlpha (0.10f + 0.25f * v));
            g.fillRect (juce::Rectangle<float> (x0, cy - v * 11.0f, x1 - x0 + 0.5f, v * 22.0f));
        }
        g.setColour (col.withAlpha (tr.active ? 0.95f : 0.6f));
        g.strokePath (band, juce::PathStrokeType (1.3f));
        g.setColour (tr.active ? theme::ivoryTrace : theme::displayLabel);
        g.setFont (theme::font (14.0f, tr.active ? 2 : 0));
        g.drawText (noteName (notes[(size_t) i], sharps, false), juce::Rectangle<float> (r.getX() + 6.0f, cy - 9.0f, 28.0f, 18.0f), juce::Justification::centred);
    }
    if (rows == 0)
    {
        g.setColour (theme::displayLabel);
        g.setFont (theme::font (13.0f));
        g.drawText ("No harmony voices. Play keys, choose CHORD, or check the no-note policy.", plot, juce::Justification::centred);
    }
    // chord readout
    g.setFont (theme::spaced (13.0f, 0.18f, 1));
    const float tw = std::max (90.0f, juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), chordText) + 28.0f);
    auto box = juce::Rectangle<float> (tw, 26.0f).withPosition (r.getRight() - tw - 12.0f, r.getY() + 10.0f);
    g.setColour (theme::display.brighter (0.05f));
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (theme::displayLabel.withAlpha (0.6f));
    g.drawRoundedRectangle (box, 4.0f, 1.0f);
    g.setColour (theme::ivoryTrace);
    g.drawText (chordText, box, juce::Justification::centred);
    // status + axis
    g.setFont (theme::spaced (10.5f, 0.1f, 1));
    g.setColour (theme::displayLabel);
    g.drawText (statusText, juce::Rectangle<float> (r.getX() + 36.0f, r.getBottom() - 18.0f, r.getWidth() - 200.0f, 14.0f), juce::Justification::centredLeft);
    g.drawText ("measured wet energy at voiced notes  -8 s ... now", juce::Rectangle<float> (r.getRight() - 330.0f, r.getBottom() - 18.0f, 318.0f, 14.0f), juce::Justification::centredRight);
}
} // namespace pa
