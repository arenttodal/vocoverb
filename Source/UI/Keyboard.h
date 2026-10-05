// On-screen harmony keyboard (shows effective voiced notes, sends notes or edits the stored chord) and wheels.
#pragma once

#include "Widgets.h"

namespace pa
{
class HarmonyKeyboard : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit HarmonyKeyboard (PluginProcessor& p);
    void setRange (int lowNote, int numWhiteKeys);
    void update();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;

private:
    int noteAt (juce::Point<float> p) const;
    juce::Rectangle<float> keyRect (int note) const;
    static bool isBlack (int n) { const int pc = n % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
    void press (int note);
    void release();
    PluginProcessor& proc;
    int low = 36, whites = 29, mouseNote = -1;
    std::array<float, 128> voiced {}, releasing {};
    std::array<bool, 128> customMark {};
    std::array<bool, 128> qwertyDown {};
    float whiteW = 20.0f;
};

class Wheel : public juce::Component, public juce::SettableTooltipClient
{
public:
    Wheel (PluginProcessor& p, bool pitch);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void send();
    PluginProcessor& proc;
    bool isPitch;
    float value; // pitch: -1..1, mod: 0..1
};
} // namespace pa
