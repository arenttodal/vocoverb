// Real-signal displays: delay envelope history, reverb energy cloud, harmony (measured wet energy at voiced notes).
#pragma once

#include "Core/FFT.h"
#include "Widgets.h"

#include <map>

namespace pa
{
/** Copies the most recent `count` history buckets (oldest first). */
void readHistory (const std::array<std::atomic<float>, Telemetry::kHistLen>& ring, int write, int count, std::vector<float>& out);

class DelayGraph : public juce::Component
{
public:
    explicit DelayGraph (PluginProcessor& p) : proc (p) {}
    void update();
    void paint (juce::Graphics&) override;
    void setOverlayText (const juce::String& s) { overlay = s; }

private:
    PluginProcessor& proc;
    std::vector<float> hist;
    double spanSec = 4.5, delayMs = 375.0;
    juce::String overlay, legend;
    bool frozen = false;
    std::vector<int> taps;
};

class ReverbGraph : public juce::Component
{
public:
    explicit ReverbGraph (PluginProcessor& p) : proc (p) {}
    void update();
    void paint (juce::Graphics&) override;
    void setOverlayText (const juce::String& s) { overlay = s; }

private:
    PluginProcessor& proc;
    std::vector<float> hist;
    double spanSec = 8.0, decaySec = 8.0;
    int histEnd = 0;
    juce::String overlay, legend;
    bool frozen = false;
};

class HarmonyGraph : public juce::Component
{
public:
    explicit HarmonyGraph (PluginProcessor& p);
    void update();
    void paint (juce::Graphics&) override;

private:
    struct Track
    {
        std::vector<float> level; // measured dB-normalised 0..1, oldest first
        bool active = false;
        double lastActive = 0.0;
    };
    float measureNote (int note) const;
    PluginProcessor& proc;
    FFT fft;
    std::vector<std::complex<float>> buf;
    std::vector<float> mag, window, wetHist;
    std::map<int, Track> tracks;
    double now = 0.0;
    double binHz = 1.0;
    float refLevel = 1.0e-3f;
    juce::String chordText, statusText;
    static constexpr int kN = 4096, kHistPoints = 200;
};
} // namespace pa
