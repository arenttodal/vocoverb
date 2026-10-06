// Delay waveform, reverb particle cloud and harmony note ribbons. All three share one recessed graph well and one
// additive "light canvas" (bright cores, fine orange detail, narrow halo); each has its own data model.
//
// Time contract: the x axis is "seconds since the capture started". A capture starts at an input onset (after
// >= 0.25 s of silence), when sound appears with no capture running, or every ~9 s while sound continues. Live
// energy fades out when the wet signal is silent; an idle well shows a subdued model preview (Delay/Reverb, derived
// from the current parameters) or faint baselines (Harmony). The reference visual fixture (visual-test mode only)
// feeds envelopes derived from the approved PNG through the same renderers.
#pragma once

#include "Widgets.h"

#include <map>
#include <optional>

namespace pa
{
/** Additive light accumulation (device resolution) plus a quarter-resolution halo layer, composited over a cached
    background image with a screen blend. Message-thread only. */
class LightCanvas
{
public:
    void configure (int logicalW, int logicalH, float scale);
    bool matches (int w, int h, float s) const noexcept { return w == lw && h == lh && std::abs (s - scale) < 0.01f; }
    void clear();
    void scaleAll (float k);
    /** vertical span at logical x (1 logical px wide) from y0 to y1; rgb are linear light amounts. */
    void span (float x, float y0, float y1, float r, float g, float b);
    void haloSpan (float x, float y0, float y1, float r, float g, float b);
    /** vertical spike around cy reaching `up` above and `dn` below; brightness falls as (1 - d/extent)^power. */
    void taper (float x, float cy, float up, float dn, float r, float g, float b, float power);
    void point (float x, float y, float size, float r, float g, float b);
    void haloPoint (float x, float y, float r, float g, float b);
    void hline (float x0, float x1, float y, float r, float g, float b, float thickness = 1.0f);
    void render (juce::Image& dest, const juce::Image& background);
    float deviceScale() const noexcept { return scale; }

private:
    void blurHalo();
    int lw = 0, lh = 0, W = 0, H = 0, hw = 0, hh = 0;
    float scale = 1.0f;
    std::vector<float> acc, halo, tmp;
    std::vector<float> powLut; float powLutP = -1.0f;
    void mark (int y, int x0, int x1) noexcept { if (y < 0 || y >= H) return; rowLo[(size_t) y] = std::min (rowLo[(size_t) y], std::max (0, x0)); rowHi[(size_t) y] = std::max (rowHi[(size_t) y], std::min (W, x1)); }
    std::vector<int> rowLo, rowHi; // lit x range per device row (everything else is the background)
    std::vector<float> bgF, haloRow; std::vector<uint8_t> bgA; const void* bgKey = nullptr; int bgW = 0, bgH = 0;
};

/** Onset-triggered capture shared by the three views (see time contract above). */
class CaptureClock
{
public:
    void update (Telemetry& t, double sampleRate);
    bool active() const noexcept { return isActive; }
    int64_t start() const noexcept { return captureStart; }
    int64_t now() const noexcept { return absNow; }
    double bucketsPerSecond() const noexcept { return bps; }
    double elapsedSeconds() const noexcept { return isActive ? (double) (absNow - captureStart) / bps : 0.0; }
    float liveAlpha() const noexcept { return alpha; }
    static int ring (int64_t abs) noexcept { return (int) (abs & (Telemetry::kHistLen - 1)); }
    bool valid (int64_t abs) const noexcept { return abs <= absNow && absNow - abs < Telemetry::kHistLen - 8 && abs >= 0; }

private:
    int lastW = -1;
    int64_t absNow = 0, captureStart = 0, lastEnergy = -1000000, silentRun = 1000000;
    bool isActive = false;
    float alpha = 0.0f;
    double bps = 48000.0 / Telemetry::kBucket;
};

/** Shared recessed well: cached surface + grid + axis labels; subclasses draw their light into the canvas. */
class GraphWell : public juce::Component
{
public:
    static inline double profDraw = 0, profRender = 0, profBlit = 0;
    explicit GraphWell (PluginProcessor& p) : proc (p) { setOpaque (false); setInterceptsMouseClicks (false, true); }
    void paint (juce::Graphics&) override;
    void resized() override { bgScale = -1.0f; dirty = true; }
    void setReferenceFixture (bool on) { fixture = on; dirty = true; }
    /** Section bypassed: the well stays dark, its light is dimmed and `text` is shown. */
    void setDimmed (bool on, const juce::String& text) { if (on != dimmed || text != dimText) { dimmed = on; dimText = text; dirty = true; repaint(); } }
    void markDirty() { dirty = true; repaint(); }

protected:
    struct Axis { float x0 = 0, pxPerSec = 100; std::vector<float> labelSeconds; float labelY = 0; };
    virtual void drawLight (LightCanvas&) = 0;
    virtual void paintOverlay (juce::Graphics&) {}
    void setGrid (std::vector<float> xs, std::vector<float> ys, std::optional<float> axisLine) { gridX = std::move (xs); gridY = std::move (ys); axisY = axisLine; bgScale = -1.0f; }
    PluginProcessor& proc;
    Axis axis;
    bool fixture = false, dirty = true, dimmed = false;
    juce::String dimText;

private:
    void buildBackground (float scale);
    std::vector<float> gridX, gridY;
    std::optional<float> axisY;
    LightCanvas canvas;
    juce::Image background, frame;
    float bgScale = -1.0f;
};

class DelayView : public GraphWell
{
public:
    DelayView (PluginProcessor& p, CaptureClock& c);
    void update();

private:
    void drawLight (LightCanvas&) override;
    void computeModel();
    CaptureClock& clock;
    float lastAlpha = -1.0f; int64_t lastNow = -1;
    std::vector<float> liveTop, liveBot, liveCore, modelTop, modelBot;
    float lastModelKey = -1.0f;
    bool frozen = false;
};

class ReverbView : public GraphWell
{
public:
    ReverbView (PluginProcessor& p, CaptureClock& c);
    void update();

private:
    struct Particle { float xn, u, b, s, ph, core, sparkle; };
    void drawLight (LightCanvas&) override;
    void computeModel();
    CaptureClock& clock;
    std::vector<Particle> field;
    float lastAlpha = -1.0f; int64_t lastNow = -1;
    std::vector<float> liveE, modelE;
    float lastModelKey = -1.0f, motionPhase = 0.0f, spreadScale = 1.0f;
};

class HarmonyView : public GraphWell
{
public:
    HarmonyView (PluginProcessor& p, CaptureClock& c);
    void update();

private:
    struct Lane { int note = -1; std::vector<float> off, up, dn, lum; float energy = 0.0f; };
    void drawLight (LightCanvas&) override;
    void paintOverlay (juce::Graphics&) override;
    float laneY (int index, int count) const;
    CaptureClock& clock;
    float lastAlpha = -1.0f; int64_t lastNow = -1; int lastVoicedKey = -1;
    std::vector<Lane> lanes;
    std::vector<int> voicedNotes;
};
} // namespace pa
