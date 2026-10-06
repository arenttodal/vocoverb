#include "Visuals.h"

#include "ReferenceFixtureData.h"

#include <cstring>

namespace pa
{
namespace
{
constexpr float kOrange[3] = { 1.00f, 0.36f, 0.09f };
constexpr float kMidOrange[3] = { 1.00f, 0.50f, 0.20f };
constexpr float kIvory[3] = { 1.00f, 0.92f, 0.80f };
constexpr float kRedHalo[3] = { 0.95f, 0.28f, 0.06f };

inline float clamp01 (float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline uint32_t hash (uint32_t x) noexcept { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
inline float rnd (uint32_t i) noexcept { return (float) (hash (i) & 0xffffff) / 16777216.0f; }
inline float gauss (uint32_t i) noexcept
{
    const float u1 = std::max (1.0e-6f, rnd (i * 2u + 1u)), u2 = rnd (i * 2u + 2u);
    return std::sqrt (-2.0f * std::log (u1)) * std::cos (6.2831853f * u2);
}

void drawAxisLabels (juce::Graphics& g, float x0, float pps, const std::vector<float>& secs, float y, float width)
{
    g.setColour (juce::Colour (0xffece6da));
    g.setFont (theme::capFont (8.6f, 1, 0.03f));
    for (float s : secs)
    {
        const float x = x0 + s * pps;
        if (x > width - 6.0f) continue;
        g.drawText (juce::String ((int) std::lround (s)) + " s", juce::Rectangle<float> (x - 4.0f, y - 2.0f, 40.0f, 12.0f), juce::Justification::centredLeft, false);
    }
}
} // namespace

// ================================================================================================ LightCanvas
void LightCanvas::configure (int logicalW, int logicalH, float s)
{
    lw = logicalW; lh = logicalH; scale = s;
    W = std::max (1, (int) std::ceil ((float) lw * s)); H = std::max (1, (int) std::ceil ((float) lh * s));
    hw = W / 4 + 2; hh = H / 4 + 2;
    acc.assign ((size_t) (3 * W * H), 0.0f);
    halo.assign ((size_t) (3 * hw * hh), 0.0f);
    tmp.assign (halo.size(), 0.0f);
    rowLo.assign ((size_t) H, W); rowHi.assign ((size_t) H, 0);
}

void LightCanvas::scaleAll (float k)
{
    for (auto& v : acc) v *= k; // rare (bypassed sections)
    for (auto& v : halo) v *= k;
}

void LightCanvas::clear()
{
    for (int y = 0; y < H; ++y)
        if (rowHi[(size_t) y] > rowLo[(size_t) y])
            std::fill (acc.begin() + 3 * (y * W + rowLo[(size_t) y]), acc.begin() + 3 * (y * W + rowHi[(size_t) y]), 0.0f);
    std::fill (halo.begin(), halo.end(), 0.0f);
    std::fill (rowLo.begin(), rowLo.end(), W); std::fill (rowHi.begin(), rowHi.end(), 0);
}

void LightCanvas::span (float x, float y0, float y1, float r, float g, float b)
{
    if (y1 < y0) std::swap (y0, y1);
    const float dx0 = x * scale, dx1 = (x + 1.0f) * scale;
    const float dy0 = std::max (0.0f, y0 * scale), dy1 = std::min ((float) H, y1 * scale);
    if (dy1 <= dy0 && dy1 - dy0 > -1.0f) { // thinner than a device pixel: deposit with fractional weight
        const int yi = (int) dy0;
        if (yi < 0 || yi >= H) return;
        const float w = std::max (0.15f, y1 * scale - y0 * scale);
        mark (yi, (int) dx0, (int) std::ceil (dx1));
        for (int xi = std::max (0, (int) dx0); xi < std::min (W, (int) std::ceil (dx1)); ++xi)
        {
            float* p = &acc[(size_t) (3 * (yi * W + xi))];
            p[0] += r * w; p[1] += g * w; p[2] += b * w;
        }
        return;
    }
    const int ya = (int) dy0, yb = std::min (H - 1, (int) std::ceil (dy1) - 1);
    for (int yi = ya; yi <= yb; ++yi) mark (yi, (int) dx0, (int) std::ceil (dx1));
    for (int xi = std::max (0, (int) dx0); xi < std::min (W, (int) std::ceil (dx1)); ++xi)
    {
        const float cx = std::min ((float) xi + 1.0f, dx1) - std::max ((float) xi, dx0);
        for (int yi = ya; yi <= yb; ++yi)
        {
            const float cy = std::min ((float) yi + 1.0f, dy1) - std::max ((float) yi, dy0);
            const float w = cx * cy;
            float* p = &acc[(size_t) (3 * (yi * W + xi))];
            p[0] += r * w; p[1] += g * w; p[2] += b * w;
        }
    }
}

void LightCanvas::taper (float x, float cy, float up, float dn, float r, float g, float b, float power)
{
    const float dx0 = x * scale, dx1 = (x + 1.0f) * scale;
    const float top = (cy - up) * scale, bot = (cy + dn) * scale, c = cy * scale;
    const int ya = std::max (0, (int) std::floor (top)), yb = std::min (H - 1, (int) std::ceil (bot));
    for (int yi = ya; yi <= yb; ++yi)
    {
        const float yc = (float) yi + 0.5f;
        const float ext = (yc < c ? up : dn) * scale;
        if (ext <= 0.0f) continue;
        const float d = std::abs (yc - c) / ext;
        if (d >= 1.0f) continue;
        if (power != powLutP)
        {
            powLut.resize (257);
            for (int i = 0; i <= 256; ++i) powLut[(size_t) i] = std::pow ((float) i / 256.0f, power);
            powLutP = power;
        }
        const float w = powLut[(size_t) ((1.0f - d) * 256.0f)];
        mark (yi, (int) dx0, (int) std::ceil (dx1));
        for (int xi = std::max (0, (int) dx0); xi < std::min (W, (int) std::ceil (dx1)); ++xi)
        {
            const float cx = std::min ((float) xi + 1.0f, dx1) - std::max ((float) xi, dx0);
            float* p = &acc[(size_t) (3 * (yi * W + xi))];
            p[0] += r * w * cx; p[1] += g * w * cx; p[2] += b * w * cx;
        }
    }
}

void LightCanvas::haloSpan (float x, float y0, float y1, float r, float g, float b)
{
    if (y1 < y0) std::swap (y0, y1);
    const float k = scale * 0.25f;
    const int xi = (int) (x * k);
    if (xi < 0 || xi >= hw) return;
    const int ya = std::max (0, (int) (y0 * k)), yb = std::min (hh - 1, (int) (y1 * k));
    const float wx = std::min (1.0f, k); // a logical column covers k halo pixels
    for (int yy = std::max (0, ya * 4 - 20); yy <= std::min (H - 1, yb * 4 + 24); ++yy) mark (yy, xi * 4 - 20, xi * 4 + 24);
    for (int yi = ya; yi <= yb; ++yi)
    {
        float* p = &halo[(size_t) (3 * (yi * hw + xi))];
        p[0] += r * wx; p[1] += g * wx; p[2] += b * wx;
    }
}

void LightCanvas::point (float x, float y, float size, float r, float g, float b)
{
    const float dx = x * scale, dy = y * scale, ds = std::max (0.6f, size * scale);
    const float x0 = dx - ds * 0.5f, y0 = dy - ds * 0.5f, x1 = x0 + ds, y1 = y0 + ds;
    for (int yi = std::max (0, (int) y0); yi < std::min (H, (int) std::ceil (y1)); ++yi)
    {
        mark (yi, (int) x0, (int) std::ceil (x1));
        const float cy = std::min ((float) yi + 1.0f, y1) - std::max ((float) yi, y0);
        for (int xi = std::max (0, (int) x0); xi < std::min (W, (int) std::ceil (x1)); ++xi)
        {
            const float w = cy * (std::min ((float) xi + 1.0f, x1) - std::max ((float) xi, x0));
            float* p = &acc[(size_t) (3 * (yi * W + xi))];
            p[0] += r * w; p[1] += g * w; p[2] += b * w;
        }
    }
}

void LightCanvas::haloPoint (float x, float y, float r, float g, float b)
{
    const float k = scale * 0.25f;
    const int xi = (int) (x * k), yi = (int) (y * k);
    if (xi < 0 || yi < 0 || xi >= hw || yi >= hh) return;
    for (int yy = std::max (0, yi * 4 - 20); yy <= std::min (H - 1, yi * 4 + 24); ++yy) mark (yy, xi * 4 - 20, xi * 4 + 24);
    float* p = &halo[(size_t) (3 * (yi * hw + xi))];
    p[0] += r; p[1] += g; p[2] += b;
}

void LightCanvas::hline (float x0, float x1, float y, float r, float g, float b, float thickness)
{
    for (int x = std::max (0, (int) x0); x < std::min (lw, (int) std::ceil (x1)); ++x)
        span ((float) x, y - thickness * 0.5f, y + thickness * 0.5f, r, g, b);
}

void LightCanvas::blurHalo()
{
    // two separable box passes (radius 2 halo px = ~8 device px), approximating a soft gaussian
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int y = 0; y < hh; ++y)
            for (int x = 0; x < hw; ++x)
                for (int c = 0; c < 3; ++c)
                {
                    float s = 0.0f; int n = 0;
                    for (int k = -2; k <= 2; ++k) { const int xx = x + k; if (xx >= 0 && xx < hw) { s += halo[(size_t) (3 * (y * hw + xx) + c)]; ++n; } }
                    tmp[(size_t) (3 * (y * hw + x) + c)] = s / (float) n;
                }
        for (int y = 0; y < hh; ++y)
            for (int x = 0; x < hw; ++x)
                for (int c = 0; c < 3; ++c)
                {
                    float s = 0.0f; int n = 0;
                    for (int k = -2; k <= 2; ++k) { const int yy = y + k; if (yy >= 0 && yy < hh) { s += tmp[(size_t) (3 * (yy * hw + x) + c)]; ++n; } }
                    halo[(size_t) (3 * (y * hw + x) + c)] = s / (float) n;
                }
    }
}

void LightCanvas::render (juce::Image& dest, const juce::Image& background)
{
    blurHalo();
    if (! dest.isValid() || dest.getWidth() != W || dest.getHeight() != H) dest = juce::Image (juce::Image::ARGB, W, H, true);
    static const std::vector<float> satLut = [] { std::vector<float> v (1025); for (int i = 0; i <= 1024; ++i) v[(size_t) i] = 1.0f - std::exp (-(float) i * 12.0f / 1024.0f); return v; }();
    // cache the background as un-premultiplied floats (rebuilt only when the background image changes)
    if (bgKey != background.getPixelData().get() || bgW != W || bgH != H)
    {
        juce::Image::BitmapData bg (background, juce::Image::BitmapData::readOnly);
        bgF.assign ((size_t) (3 * W * H), 0.0f); bgA.assign ((size_t) (W * H), 0);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const auto c = bg.getPixelColour (std::min (x, bg.width - 1), std::min (y, bg.height - 1));
                const size_t i = (size_t) (y * W + x);
                bgA[i] = c.getAlpha();
                bgF[3 * i] = c.getFloatRed(); bgF[3 * i + 1] = c.getFloatGreen(); bgF[3 * i + 2] = c.getFloatBlue();
            }
        bgKey = background.getPixelData().get(); bgW = W; bgH = H;
    }
    juce::Image::BitmapData out (dest, juce::Image::BitmapData::writeOnly);
    juce::Image::BitmapData bgd (background, juce::Image::BitmapData::readOnly);
    haloRow.resize ((size_t) (3 * hw));
    const float hk = 0.25f;
    for (int y = 0; y < H; ++y)
    {
        const float hy = (float) y * hk - 0.5f;
        const int hy0 = juce::jlimit (0, hh - 1, (int) std::floor (hy)), hy1 = std::min (hh - 1, hy0 + 1);
        const float fy = juce::jlimit (0.0f, 1.0f, hy - (float) hy0);
        uint8_t* drow = out.getLinePointer (y);
        if (bgd.width == W && bgd.pixelStride == out.pixelStride) std::memcpy (drow, bgd.getLinePointer (std::min (y, bgd.height - 1)), (size_t) (W * out.pixelStride));
        const int lo = rowLo[(size_t) y], hi = rowHi[(size_t) y];
        if (hi <= lo && bgd.width == W) continue;
        for (int i = 0; i < 3 * hw; ++i) haloRow[(size_t) i] = halo[(size_t) (3 * hy0 * hw + i)] * (1.0f - fy) + halo[(size_t) (3 * hy1 * hw + i)] * fy;
        const float* arow = &acc[(size_t) (3 * y * W)];
        const float* brow = &bgF[(size_t) (3 * y * W)];
        const uint8_t* barow = &bgA[(size_t) (y * W)];
        const int xa = bgd.width == W ? lo : 0, xb = bgd.width == W ? hi : W;
        for (int x = xa; x < xb; ++x)
        {
            auto* dp = reinterpret_cast<juce::PixelARGB*> (drow + out.pixelStride * x);
            const uint8_t a = barow[x];
            if (a == 0) { dp->setARGB (0, 0, 0, 0); continue; }
            const float hx = (float) x * hk - 0.5f;
            const int hx0 = hx <= 0.0f ? 0 : std::min (hw - 1, (int) hx), hx1 = std::min (hw - 1, hx0 + 1);
            const float fx = hx <= 0.0f ? 0.0f : std::min (1.0f, hx - (float) hx0);
            const float* h0 = &haloRow[(size_t) (3 * hx0)]; const float* h1 = &haloRow[(size_t) (3 * hx1)];
            const float* p = arow + 3 * x;
            const float l0 = p[0] + h0[0] + (h1[0] - h0[0]) * fx, l1 = p[1] + h0[1] + (h1[1] - h0[1]) * fx, l2 = p[2] + h0[2] + (h1[2] - h0[2]) * fx;
            const float m = std::max ({ l0, l1, l2 });
            const float* bb = brow + 3 * x;
            const float af = (float) a * (1.0f / 255.0f);
            float r = bb[0], g = bb[1], b = bb[2];
            if (m > 0.002f)
            {
                // hue-preserving tone map: the brightest channel saturates smoothly, the others keep their ratio,
                // so dense orange stays orange (whiteness only comes from explicitly ivory cores)
                const float f = satLut[(size_t) std::min (1024.0f, m * (1024.0f / 12.0f))] / m;
                r = 1.0f - (1.0f - r) * (1.0f - std::min (1.0f, l0 * f));
                g = 1.0f - (1.0f - g) * (1.0f - std::min (1.0f, l1 * f));
                b = 1.0f - (1.0f - b) * (1.0f - std::min (1.0f, l2 * f));
            }
            const float k = af * 255.0f;
            dp->setARGB (a, (uint8_t) (r * k + 0.5f), (uint8_t) (g * k + 0.5f), (uint8_t) (b * k + 0.5f));
        }
    }
}

// ================================================================================================ CaptureClock
void CaptureClock::update (Telemetry& t, double sampleRate)
{
    bps = std::max (1000.0, sampleRate) / (double) Telemetry::kBucket;
    const int w = t.histWrite.load (std::memory_order_relaxed);
    int64_t from;
    if (lastW < 0)
    {
        absNow = (int64_t) Telemetry::kHistLen * 4 + w;
        from = absNow - (int64_t) (bps * 9.0); // replay the last 9 s so an editor opened mid-tail shows it
    }
    else
    {
        from = absNow;
        absNow += (w - lastW + Telemetry::kHistLen) & (Telemetry::kHistLen - 1);
    }
    lastW = w;
    const auto silenceNeeded = (int64_t) (bps * 0.25);
    for (int64_t b = from; b < absNow; ++b)
    {
        const int i = ring (b);
        const float dry = t.histDry[(size_t) i].load (std::memory_order_relaxed);
        const float wet = t.histWet[(size_t) i].load (std::memory_order_relaxed);
        if (dry > 0.0063f) // -44 dBFS input onset after a pause
        {
            if (silentRun >= silenceNeeded && (! isActive || (double) (b - captureStart) > bps * 1.0)) { captureStart = b; isActive = true; }
            silentRun = 0;
        }
        else ++silentRun;
        if (wet > 1.0e-4f)
        {
            if (! isActive || (double) (b - lastEnergy) > bps * 1.5) { captureStart = b; isActive = true; }
            lastEnergy = b;
        }
        if (isActive && (double) (b - captureStart) > bps * 9.2 && (double) (b - lastEnergy) < bps * 0.3) captureStart = b; // continuous sound
    }
    const bool sounding = isActive && (double) (absNow - lastEnergy) < bps * 1.2;
    alpha += ((sounding ? 1.0f : 0.0f) - alpha) * 0.10f;
    if (alpha < 0.002f && ! sounding) alpha = 0.0f;
}

// ================================================================================================ GraphWell
void GraphWell::buildBackground (float scale)
{
    const int W = std::max (1, (int) std::ceil ((float) getWidth() * scale)), H = std::max (1, (int) std::ceil ((float) getHeight() * scale));
    background = juce::Image (juce::Image::ARGB, W, H, true);
    {
        juce::Graphics g (background);
        g.addTransform (juce::AffineTransform::scale (scale));
        const auto r = getLocalBounds().toFloat();
        juce::Path p; p.addRoundedRectangle (r, 10.0f);
        g.setGradientFill (juce::ColourGradient (theme::displayTop, 0, 0, theme::display, 0, r.getBottom(), false));
        g.fillPath (p);
        g.reduceClipRegion (p);
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY() - r.getHeight() * 0.1f,
                                                 juce::Colours::black.withAlpha (0.20f), r.getX() - 10.0f, r.getBottom() + 10.0f, true));
        g.fillRect (r);
        g.setColour (juce::Colour (0x18e8e2d4));
        for (float x : gridX) g.fillRect (juce::Rectangle<float> (x - 0.5f, 0.0f, 1.0f, r.getHeight()));
        for (float y : gridY) g.fillRect (juce::Rectangle<float> (0.0f, y - 0.5f, r.getWidth(), 1.0f));
        if (axisY) { g.setColour (juce::Colour (0x22e8e2d4)); g.fillRect (juce::Rectangle<float> (0.0f, *axisY - 0.5f, r.getWidth(), 1.0f)); }
        // recessed edge
        g.setColour (juce::Colour (0x700a0e0e));
        g.strokePath (p, juce::PathStrokeType (1.6f));
    }
    canvas.configure (getWidth(), getHeight(), scale);
    bgScale = scale;
    dirty = true;
}

void GraphWell::paint (juce::Graphics& g)
{
    if (getWidth() < 8 || getHeight() < 8) return;
    // light is composited at the device resolution (crisp cores on Retina; resampling in the blit costs more)
    const float scale = juce::jlimit (1.0f, 3.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (std::abs (scale - bgScale) > 0.01f || ! background.isValid()) buildBackground (scale);
    if (dirty)
    {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        canvas.clear();
        drawLight (canvas);
        if (dimmed) canvas.scaleAll (0.3f);
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        canvas.render (frame, background);
        profDraw += t1 - t0; profRender += juce::Time::getMillisecondCounterHiRes() - t1;
        dirty = false;
    }
    const auto t2 = juce::Time::getMillisecondCounterHiRes();
    g.drawImage (frame, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    profBlit += juce::Time::getMillisecondCounterHiRes() - t2;
    // fine light lower rim of the recess
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.fillRect (juce::Rectangle<float> (10.0f, (float) getHeight() - 0.6f, (float) getWidth() - 20.0f, 0.6f));
    drawAxisLabels (g, axis.x0, axis.pxPerSec, axis.labelSeconds, axis.labelY, (float) getWidth());
    paintOverlay (g);
    if (dimmed && dimText.isNotEmpty())
    {
        g.setColour (theme::displayLabel.withAlpha (0.75f));
        g.setFont (theme::capFont (9.5f, 1, 0.1f));
        g.drawText (dimText, getLocalBounds().toFloat().withTrimmedBottom (24.0f), juce::Justification::centred);
    }
}

// ================================================================================================ DelayView
// Well 704 x 215 (canvas x 42..745, y 170..384). 0 s at local x 18.5, 152 px per second, trace centre y 107.5.
namespace dgeo { constexpr float x0 = 18.5f, pps = 152.0f, cy = 107.5f, amp = 92.0f; }

DelayView::DelayView (PluginProcessor& p, CaptureClock& c) : GraphWell (p), clock (c)
{
    std::vector<float> gx;
    for (int k = 1; k <= 8; ++k) gx.push_back (dgeo::x0 + 76.0f * (float) k);
    setGrid (gx, { 40.0f, 72.5f, 107.5f, 140.0f, 171.5f }, 189.5f);
    axis = { dgeo::x0, dgeo::pps, { 0, 1, 2, 3, 4 }, 198.0f };
    setTitle ("Delay display");
    setDescription ("Delay output since the last capture start (input onset). Idle: a dim preview of the current repeat pattern.");
}

void DelayView::computeModel()
{
    const auto& P = proc;
    const bool interval = P.value (DelayMode) > 0.5f;
    const bool sync = P.value (interval ? IvSync : BbdSync) > 0.5f;
    const double bpm = std::max (40.0, (double) P.value (Tempo));
    double T = sync ? divisionBeats ((int) P.value (interval ? IvDiv : BbdDiv)) * 60.0 / bpm
                    : (double) P.value (interval ? IvTime : BbdTime) / 1000.0;
    T = std::max (0.03, T);
    const float fb = P.value (interval ? IvFeedback : BbdFeedback) / 100.0f;
    const float tone = P.value (interval ? IvTone : BbdTone);
    const float lvl = juce::Decibels::decibelsToGain (P.value (interval ? IvLevel : BbdLevel));
    const float key = (float) T * 1000.0f + fb * 7.0f + tone * 0.001f + lvl + (interval ? 0.5f : 0.0f);
    if (std::abs (key - lastModelKey) < 1.0e-4f && ! modelTop.empty()) return;
    lastModelKey = key;
    const int W = getWidth();
    modelTop.assign ((size_t) W, 0.0f); modelBot.assign ((size_t) W, 0.0f);
    const float dull = juce::jlimit (0.3f, 1.0f, std::log2 (tone / 300.0f) / 6.0f); // darker tone -> finer, lower repeats
    for (int x = 0; x < W; ++x)
    {
        const double t = (x - dgeo::x0) / dgeo::pps;
        if (t < 0.0) continue;
        float a = 0.0f;
        // excitation burst (0..0.3 s), then repeats every T decaying by feedback
        if (t < 0.32) a = std::max (a, 0.85f * (float) std::sin (juce::MathConstants<double>::pi * t / 0.32));
        for (int k = 1; k < 40; ++k)
        {
            const double tk = k * T;
            if (tk > t + 0.2) break;
            const double d = (t - tk) / 0.11;
            if (d < -1.0 || d > 1.6) continue;
            const float env = (float) std::exp (-d * d * 2.2);
            a = std::max (a, std::min (1.0f, lvl * 2.2f) * std::pow (fb, (float) (k - 1)) * std::pow (dull, (float) k * 0.5f) * env);
        }
        const float n = 0.55f + 0.45f * rnd ((uint32_t) x * 7u + 3u);
        modelTop[(size_t) x] = a * n * dgeo::amp;
        modelBot[(size_t) x] = a * (0.55f + 0.45f * rnd ((uint32_t) x * 11u + 5u)) * dgeo::amp;
    }
}

void DelayView::update()
{
    const float keyBefore = lastModelKey;
    computeModel();
    const bool idle = clock.liveAlpha() == 0.0f && lastAlpha == 0.0f && keyBefore == lastModelKey && ! fixture;
    lastAlpha = clock.liveAlpha();
    if (idle && lastNow >= 0) return; // nothing new to draw (silence, unchanged model)
    lastNow = clock.now();
    const int W = getWidth();
    liveTop.assign ((size_t) W, 0.0f); liveBot.assign ((size_t) W, 0.0f); liveCore.assign ((size_t) W, 0.0f);
    auto& t = proc.telemetry();
    frozen = t.delayFrozen.load();
    if (clock.active())
    {
        const double bps = clock.bucketsPerSecond();
        for (int x = 0; x < W; ++x)
        {
            const double t0 = (x - dgeo::x0) / dgeo::pps, t1 = (x + 1 - dgeo::x0) / dgeo::pps;
            if (t1 <= 0.0) continue;
            const int64_t b0 = clock.start() + (int64_t) std::floor (std::max (0.0, t0) * bps);
            const int64_t b1 = std::max (b0 + 1, clock.start() + (int64_t) std::floor (t1 * bps));
            float mn = 0.0f, mx = 0.0f; bool any = false;
            for (int64_t b = b0; b < b1; ++b)
            {
                if (! clock.valid (b) || b >= clock.now()) continue;
                const int i = CaptureClock::ring (b);
                mn = std::min (mn, t.histDelayMin[(size_t) i].load (std::memory_order_relaxed));
                mx = std::max (mx, t.histDelayMax[(size_t) i].load (std::memory_order_relaxed));
                any = true;
            }
            if (! any) continue;
            auto f = [] (float a) { return dgeo::amp * std::pow (clamp01 (a / 0.55f), 0.72f); };
            liveTop[(size_t) x] = f (mx);
            liveBot[(size_t) x] = f (-mn);
            liveCore[(size_t) x] = clamp01 (std::max (mx, -mn) * 2.4f);
        }
    }
    markDirty();
}

void DelayView::drawLight (LightCanvas& c)
{
    const int W = getWidth();
    const float cy = dgeo::cy;
    auto drawTrace = [&] (auto topAt, auto botAt, auto coreAt, float gain)
    {
        float neighbour = 0.0f;
        for (int x = 0; x < W; ++x)
        {
            const float top = topAt (x), bot = botAt (x), core = coreAt (x);
            const float ext = std::max (top, bot);
            neighbour = std::max (neighbour * 0.97f, ext / dgeo::amp);
            // baseline: pale, warmer and brighter near energised parts, fading to the right
            const float fade = 1.0f - 0.55f * (float) x / (float) W;
            const float bl = (0.8f + 1.6f * neighbour) * fade * gain;
            const float mixO = clamp01 (neighbour * 2.2f);
            c.span ((float) x, cy - 0.55f, cy + 0.55f, bl * (kIvory[0] * (1 - mixO) + kMidOrange[0] * mixO), bl * (kIvory[1] * (1 - mixO) + kMidOrange[1] * mixO),
                    bl * (kIvory[2] * (1 - mixO) + kMidOrange[2] * mixO));
            if (ext < 0.6f) continue;
            const float e = ext / dgeo::amp;
            // narrow halo (slightly broader around the strongest region)
            const float hg = 0.05f * gain * (0.3f + e);
            c.haloSpan ((float) x, cy - top * 0.9f, cy + bot * 0.9f, kOrange[0] * hg, kOrange[1] * hg, kOrange[2] * hg);
            // fine orange vertical detail: a spike tapering to its tip
            const float hair = 0.25f + 0.75f * rnd ((uint32_t) x * 31u + 7u);
            const float io = (1.0f + 2.0f * e) * gain * hair;
            c.taper ((float) x, cy, top, bot, kOrange[0] * io, kOrange[1] * io, kOrange[2] * io, 1.25f);
            // pale peach core hugging the centre line
            const float ic = (0.3f + 2.4f * core) * gain * (0.6f + 0.4f * hair);
            c.taper ((float) x, cy, top * 0.4f, bot * 0.4f, kIvory[0] * ic, kIvory[1] * ic * 0.85f, kIvory[2] * ic * 0.65f, 2.2f);
        }
    };
    if (fixture)
    {
        drawTrace ([] (int x) { return x < 704 ? fixture::kDelay[x][0] * 1.02f : 0.0f; }, [] (int x) { return x < 704 ? fixture::kDelay[x][1] * 1.02f : 0.0f; },
                   [] (int x) { return x < 704 ? clamp01 ((fixture::kDelay[x][2] - 0.4f) * 2.2f) : 0.0f; }, 1.8f);
        return;
    }
    const float live = clock.liveAlpha();
    if (live < 0.999f && ! modelTop.empty())
        drawTrace ([this] (int x) { return modelTop[(size_t) x]; }, [this] (int x) { return modelBot[(size_t) x]; },
                   [this] (int x) { return modelTop[(size_t) x] / dgeo::amp * 0.6f; }, 0.30f * (1.0f - live));
    if (live > 0.001f && ! liveTop.empty())
        drawTrace ([this] (int x) { return liveTop[(size_t) x]; }, [this] (int x) { return liveBot[(size_t) x]; },
                   [this] (int x) { return liveCore[(size_t) x]; }, live);
}

// ================================================================================================ ReverbView
// Well 704 x 215 (canvas x 791..1494). 0 s at local x 19, 78.1 px per second (grid every second), centre y 110.
namespace rgeo { constexpr float x0 = 19.0f, pps = 78.1f, cy = 110.0f; }

ReverbView::ReverbView (PluginProcessor& p, CaptureClock& c) : GraphWell (p), clock (c)
{
    std::vector<float> gx;
    for (int k = 1; k <= 8; ++k) gx.push_back (rgeo::x0 + rgeo::pps * (float) k);
    setGrid (gx, { 38.5f, 72.0f, 108.0f, 139.5f, 171.0f }, 189.5f);
    axis = { rgeo::x0, rgeo::pps, { 0, 2, 4, 6, 8 }, 198.0f };
    // deterministic stratified particle field (normalised x, gaussian vertical offset, threshold, size, phase)
    constexpr int N = 60000;
    field.reserve (N);
    for (int i = 0; i < N; ++i)
    {
        const float u = juce::jlimit (-2.8f, 2.8f, gauss ((uint32_t) i + 77u));
        const float ph = 6.2831853f * rnd ((uint32_t) i * 17u + 4u);
        field.push_back ({ ((float) i + rnd ((uint32_t) i * 3u + 1u)) / (float) N, u, rnd ((uint32_t) i * 5u + 9u), 0.55f + 0.75f * rnd ((uint32_t) i * 13u + 2u), ph,
                           std::exp (-0.5f * u * u), 0.35f + 1.3f * rnd ((uint32_t) (ph * 1000.0f) + 11u) });
    }
    setTitle ("Reverb display");
    setDescription ("Reverb output energy since the last capture start, drawn as a diffuse point cloud. Idle: a dim preview of the current decay.");
}

void ReverbView::computeModel()
{
    const bool wash = proc.value (ReverbMode) > 0.5f;
    const float decay = proc.value (wash ? WaDecay : PlDecay);
    const float bloom = wash ? proc.value (WaBloom) / 100.0f : 0.05f;
    const float key = decay * 3.0f + bloom * 11.0f + (wash ? 0.5f : 0.0f);
    spreadScale = wash ? 0.85f + 0.3f * bloom : 0.75f;
    if (std::abs (key - lastModelKey) < 1.0e-4f && ! modelE.empty()) return;
    lastModelKey = key;
    const int W = getWidth();
    modelE.assign ((size_t) W, 0.0f);
    const float tau = 0.02f + bloom * 0.7f;
    for (int x = 0; x < W; ++x)
    {
        const float t = ((float) x - rgeo::x0) / rgeo::pps;
        if (t < 0.0f) continue;
        modelE[(size_t) x] = (1.0f - std::exp (-t / tau)) * std::pow (10.0f, -3.0f * t / std::max (0.3f, decay)) * (1.0f - 0.25f * bloom * std::exp (-t / (tau + 0.4f)));
    }
}

void ReverbView::update()
{
    const float keyBefore = lastModelKey;
    computeModel();
    const bool idle = clock.liveAlpha() == 0.0f && lastAlpha == 0.0f && keyBefore == lastModelKey && ! fixture;
    lastAlpha = clock.liveAlpha();
    if (idle && lastNow >= 0) return;
    lastNow = clock.now();
    motionPhase += 0.012f * (0.2f + proc.value (proc.value (ReverbMode) > 0.5f ? WaMotion : PlMotion) / 100.0f);
    const int W = getWidth();
    liveE.assign ((size_t) W, 0.0f);
    auto& t = proc.telemetry();
    if (clock.active())
    {
        const double bps = clock.bucketsPerSecond();
        for (int x = 0; x < W; ++x)
        {
            const double t0 = (x - rgeo::x0) / rgeo::pps, t1 = (x + 1 - rgeo::x0) / rgeo::pps;
            if (t1 <= 0.0) continue;
            const int64_t b0 = clock.start() + (int64_t) std::floor (std::max (0.0, t0) * bps);
            const int64_t b1 = std::max (b0 + 1, clock.start() + (int64_t) std::floor (t1 * bps));
            float a = 0.0f; bool any = false;
            for (int64_t b = b0; b < b1; ++b)
            {
                if (! clock.valid (b) || b >= clock.now()) continue;
                const int i = CaptureClock::ring (b);
                a = std::max ({ a, t.histReverbMax[(size_t) i].load (std::memory_order_relaxed), -t.histReverbMin[(size_t) i].load (std::memory_order_relaxed) });
                any = true;
            }
            if (any) liveE[(size_t) x] = std::pow (clamp01 (a / 0.42f), 0.6f);
        }
    }
    markDirty();
}

void ReverbView::drawLight (LightCanvas& c)
{
    const int W = getWidth();
    struct Col { float e, cy, sd, warm; };
    auto colAt = [&] (int x, const std::vector<float>& E, float gainSpread) -> Col
    {
        const float e = E[(size_t) x];
        const float t = ((float) x - rgeo::x0) / rgeo::pps;
        const float sd = gainSpread * (12.0f + 46.0f * std::sqrt (e)) * (1.0f - 0.04f * std::max (0.0f, t));
        const float warm = clamp01 (0.75f - t * 0.16f) * (0.5f + 0.5f * e);
        return { e, rgeo::cy, sd, warm };
    };
    auto drawCloud = [&] (auto colFn, float gain, float density)
    {
        // subdued low-frequency glow under the body
        for (int x = 0; x < W; x += 1)
        {
            const Col k = colFn (x);
            if (k.e < 0.004f) continue;
            const float g0 = 0.11f * gain * k.e * (0.3f + 0.7f * k.warm);
            c.haloSpan ((float) x, k.cy - k.sd * 1.25f, k.cy + k.sd * 1.25f, g0 * (kOrange[0] * k.warm + kIvory[0] * (1 - k.warm)),
                        g0 * (kOrange[1] * k.warm + kIvory[1] * (1 - k.warm)), g0 * (kOrange[2] * k.warm + kIvory[2] * (1 - k.warm)));
        }
        // fine point texture with small bright cores
        for (const auto& pt : field)
        {
            const int x = (int) (pt.xn * (float) W);
            if (x < 0 || x >= W) continue;
            const Col k = colFn (x);
            if (k.e < 0.004f || pt.b > std::pow (k.e, 1.5f) * density) continue;
            const float wob = 0.06f * std::sin (motionPhase * 6.0f + pt.ph);
            const float y = k.cy + (pt.u + wob) * k.sd;
            if (y < 2.0f || y > (float) getHeight() - 26.0f) continue;
            const float core = pt.core, sparkle = pt.sparkle;
            const float inten = gain * (0.45f + 2.0f * core) * (0.2f + 1.3f * k.e) * sparkle;
            const float w = clamp01 (k.warm * (1.08f - 0.3f * core * k.e));
            c.point (pt.xn * (float) W, y, pt.s, inten * (kOrange[0] * w + kIvory[0] * (1 - w)), inten * (kOrange[1] * w + kIvory[1] * (1 - w)),
                     inten * (kOrange[2] * w + kIvory[2] * (1 - w)));
        }
    };
    if (fixture)
    {
        drawCloud ([this] (int x) -> Col {
            if (x < 4 || x >= 700) return { 0, rgeo::cy, 0, 0 };
            const auto& f = fixture::kReverb[x];
            const float t = ((float) x - rgeo::x0) / rgeo::pps;
            const float warm = clamp01 (0.35f * clamp01 (f[3] * 3.0f) + 0.75f * clamp01 (1.05f - t / 3.6f));
            return { clamp01 (f[0] * 1.15f), f[1] - 170.0f, f[2] * (1.0f + 0.25f * clamp01 (1.0f - t / 2.0f)), warm };
        }, 1.5f, 3.6f);
        return;
    }
    const float live = clock.liveAlpha();
    if (live < 0.999f && ! modelE.empty())
        drawCloud ([&] (int x) { return colAt (x, modelE, spreadScale); }, 0.32f * (1.0f - live), 1.4f);
    if (live > 0.001f && ! liveE.empty())
        drawCloud ([&] (int x) { return colAt (x, liveE, spreadScale); }, live, 2.0f);
}

// ================================================================================================ HarmonyView
// Well 912 x 160 (canvas x 42..953, y 686..845). 0 s at local x 63, 102.75 px per second, lanes around y 74.
namespace hgeo { constexpr float x0 = 63.0f, pps = 102.75f, amp = 14.0f; }

HarmonyView::HarmonyView (PluginProcessor& p, CaptureClock& c) : GraphWell (p), clock (c)
{
    std::vector<float> gx;
    for (int k = 1; k <= 8; ++k) gx.push_back (hgeo::x0 + hgeo::pps * (float) k);
    setGrid (gx, { 27.0f, 128.0f }, std::nullopt);
    axis = { hgeo::x0, hgeo::pps, { 0, 2, 4, 6, 8 }, 142.0f };
    setTitle ("Harmony display");
    setDescription ("One ribbon per voiced note: the Classic synthesis band nearest each note, since the last capture start.");
}

float HarmonyView::laneY (int index, int count) const
{
    if (count <= 0) return 73.5f;
    const float spacing = count <= 3 ? 33.5f : std::min (33.5f, 112.0f / (float) count);
    return 73.5f + ((float) index - 0.5f * (float) (count - 1)) * spacing;
}

void HarmonyView::update()
{
    auto& t = proc.telemetry();
    voicedNotes.clear();
    for (int v = 0; v < kMaxVoices; ++v)
    {
        const int n = t.voiceNote[(size_t) v].load();
        if (n >= 0 && t.voiceEnv[(size_t) v].load() > 0.002f && std::find (voicedNotes.begin(), voicedNotes.end(), n) == voicedNotes.end()) voicedNotes.push_back (n);
    }
    int vk = 0;
    for (int n : voicedNotes) vk = vk * 131 + n + 1;
    const bool idle = clock.liveAlpha() == 0.0f && lastAlpha == 0.0f && vk == lastVoicedKey && ! fixture;
    lastAlpha = clock.liveAlpha(); lastVoicedKey = vk;
    if (idle && lastNow >= 0) return;
    lastNow = clock.now();
    const int W = getWidth();
    std::map<int, Lane> map;
    if (clock.active())
    {
        const double bps = clock.bucketsPerSecond();
        for (int x = 0; x < W; ++x)
        {
            const double t0 = (x - hgeo::x0) / hgeo::pps, t1 = (x + 1 - hgeo::x0) / hgeo::pps;
            if (t1 <= 0.0) continue;
            const int64_t b0 = clock.start() + (int64_t) std::floor (std::max (0.0, t0) * bps);
            const int64_t b1 = std::max (b0 + 1, clock.start() + (int64_t) std::floor (t1 * bps));
            for (int64_t b = b0; b < b1; ++b)
            {
                if (! clock.valid (b) || b >= clock.now()) continue;
                const int i = CaptureClock::ring (b);
                for (int v = 0; v < Telemetry::kLanes; ++v)
                {
                    const int n = t.histLaneNote[(size_t) v][(size_t) i].load (std::memory_order_relaxed);
                    if (n < 0) continue;
                    auto& L = map[n];
                    if (L.up.empty()) { L.note = n; L.off.assign ((size_t) W, 0.0f); L.up.assign ((size_t) W, 0.0f); L.dn.assign ((size_t) W, 0.0f); L.lum.assign ((size_t) W, 0.0f); }
                    const float mx = t.histLaneMax[(size_t) v][(size_t) i].load (std::memory_order_relaxed);
                    const float mn = t.histLaneMin[(size_t) v][(size_t) i].load (std::memory_order_relaxed);
                    auto f = [] (float a) { return hgeo::amp * std::pow (clamp01 (a / 0.03f), 0.6f); };
                    L.up[(size_t) x] = std::max (L.up[(size_t) x], f (mx));
                    L.dn[(size_t) x] = std::max (L.dn[(size_t) x], f (-mn));
                    L.lum[(size_t) x] = std::max (L.lum[(size_t) x], clamp01 ((mx - mn) * 18.0f));
                }
            }
        }
        // energy contour: the core rises gently with the smoothed band energy
        for (auto& [n, L] : map)
        {
            float s = 0.0f;
            for (int x = 0; x < W; ++x) { s += (0.5f * (L.up[(size_t) x] + L.dn[(size_t) x]) - s) * 0.08f; L.off[(size_t) x] = -0.45f * s; L.energy = std::max (L.energy, s); }
        }
    }
    for (int n : voicedNotes) if (! map.count (n)) { Lane L; L.note = n; map[n] = L; }
    lanes.clear();
    for (auto it = map.rbegin(); it != map.rend(); ++it) lanes.push_back (it->second); // highest note on top
    if (lanes.size() > (size_t) kMaxVoices) lanes.resize ((size_t) kMaxVoices);
    markDirty();
}

void HarmonyView::drawLight (LightCanvas& c)
{
    const int W = getWidth();
    auto ribbon = [&] (float y0, auto offAt, auto upAt, auto dnAt, auto lumAt, float gain, float xStart)
    {
        for (int x = (int) xStart; x < W - 4; ++x)
        {
            const float off = offAt (x), up = upAt (x), dn = dnAt (x), lum = lumAt (x);
            const float yc = y0 + off;
            const float e = std::max (up, dn) / hgeo::amp;
            // pale narrow core line, always present while the voice exists
            const float ic = gain * (0.9f + 2.2f * lum);
            c.span ((float) x, yc - 0.75f, yc + 0.75f, 1.0f * ic, 0.66f * ic, 0.42f * ic);
            const float hb = gain * (0.035f + 0.07f * lum);
            c.haloSpan ((float) x, yc - 6.0f, yc + 6.0f, kOrange[0] * hb, kOrange[1] * hb, kOrange[2] * hb);
            if (up + dn < 0.5f) continue;
            const float hair = 0.35f + 0.65f * rnd ((uint32_t) x * 37u + (uint32_t) (y0 * 13.0f));
            const float io = gain * (0.45f + 1.2f * e) * hair;
            c.taper ((float) x, yc, up, dn, kOrange[0] * io, kOrange[1] * io, kOrange[2] * io, 1.1f);
            const float hg = 0.05f * gain * (0.4f + e);
            c.haloSpan ((float) x, yc - up * 0.8f, yc + dn * 0.8f, kOrange[0] * hg, kOrange[1] * hg, kOrange[2] * hg);
        }
    };
    if (fixture)
    {
        const float* tabs[3] = { &fixture::kLaneG[0][0], &fixture::kLaneEb[0][0], &fixture::kLaneC[0][0] };
        for (int l = 0; l < 3; ++l)
        {
            const float* d = tabs[l];
            auto at = [d] (int x, int k) { return x < 912 ? d[x * 4 + k] : 0.0f; };
            ribbon (laneY (l, 3), [&] (int x) { return at (x, 0); }, [&] (int x) { return at (x, 1) * 0.9f; }, [&] (int x) { return at (x, 2) * 0.9f; },
                    [&] (int x) { return clamp01 ((at (x, 3) - 0.4f) * 1.6f); }, 1.0f, 50.0f);
        }
        return;
    }
    const int n = (int) lanes.size();
    const float live = clock.liveAlpha();
    for (int i = 0; i < n; ++i)
    {
        const auto& L = lanes[(size_t) i];
        const float y = laneY (i, n);
        if (L.up.empty() || live < 0.01f)
        {
            // voiced but silent: subtle baseline only (no fake energy)
            c.hline (50.0f, (float) W - 6.0f, y, 0.16f, 0.13f, 0.10f, 1.0f);
            continue;
        }
        ribbon (y, [&L] (int x) { return L.off[(size_t) x]; }, [&L] (int x) { return L.up[(size_t) x]; }, [&L] (int x) { return L.dn[(size_t) x]; },
                [&L] (int x) { return L.lum[(size_t) x]; }, live, hgeo::x0 - 12.0f);
    }
}

void HarmonyView::paintOverlay (juce::Graphics& g)
{
    std::vector<int> notes;
    if (fixture) notes = { 55, 51, 48 };
    else for (auto& L : lanes) notes.push_back (L.note);
    if (notes.empty())
    {
        g.setColour (theme::displayLabel.withAlpha (0.45f));
        g.setFont (theme::capFont (9.0f, 1, 0.12f));
        g.drawText ("NO NOTES", getLocalBounds().toFloat().withTrimmedBottom (20), juce::Justification::centred);
        return;
    }
    const bool sharps = keyPrefersSharps ((int) proc.value (ChRoot));
    std::vector<int> pcs;
    bool dupPc = false;
    for (int nn : notes) { if (std::find (pcs.begin(), pcs.end(), nn % 12) != pcs.end()) dupPc = true; pcs.push_back (nn % 12); }
    g.setColour (theme::ivoryTrace.withAlpha (0.95f));
    for (int i = 0; i < (int) notes.size(); ++i)
    {
        const float y = laneY (i, (int) notes.size());
        g.setFont (theme::capFont (notes.size() > 4 ? 8.5f : 10.5f, 0));
        g.drawText (noteName (notes[(size_t) i], sharps, dupPc), juce::Rectangle<float> (17.0f, y - 9.0f, 34.0f, 18.0f), juce::Justification::centredLeft, false);
    }
}
} // namespace pa
