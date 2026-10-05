// Interface the editor uses to reach standalone-only services (devices, live input). Null inside plugins.
#pragma once

#include <JuceHeader.h>

namespace pa
{
struct StandaloneHost
{
    virtual ~StandaloneHost() = default;
    virtual void showAudioMidiSettings() = 0;
    /** Enables device input. The OS microphone permission prompt can appear here (never at launch). */
    virtual void setLiveInput (bool enabled) = 0;
    virtual bool liveInputActive() const = 0;
    virtual juce::String deviceSummary() const = 0;
    virtual juce::StringArray enabledMidiInputs() const = 0;

    static StandaloneHost*& instance() { static StandaloneHost* h = nullptr; return h; }
};
} // namespace pa
