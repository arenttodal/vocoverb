// Preset browser model: embedded factory presets, user presets (JSON, atomic saves), favourites, A/B snapshots.
#pragma once

#include <JuceHeader.h>

#include "Core/Params.h"
#include "FactoryPresets.h"

namespace pa
{
class PluginProcessor;

class PresetManager
{
public:
    static constexpr int kPresetSchema = 1;
    explicit PresetManager (PluginProcessor& p);

    struct Entry { juce::String name, category, description; bool factory = true; int factoryIndex = -1; juce::File file; };
    const juce::Array<Entry>& entries();   // factory first, then user presets (rescanned on demand)
    void rescan();
    int currentIndex() const noexcept { return current; }
    juce::String currentName() const { return currentPresetName; }
    void setCurrentName (const juce::String& n) { currentPresetName = n; }

    void loadFactory (int index, bool notify = true);
    bool loadEntry (int index);
    bool loadFile (const juce::File& f, juce::String& error);
    void step (int delta);

    enum class Collision { Ask, Replace, KeepBoth };
    /** Saves current state. Returns false with error text. If the name exists and policy is Ask, returns false with error "exists". */
    bool saveUser (const juce::String& name, Collision policy, juce::String& error);
    juce::var toJson (const juce::String& name) const;
    bool fromJson (const juce::var& v, juce::String& error);

    static juce::File userPresetDirectory();
    bool isFavourite (const juce::String& name) const { return favourites.contains (name); }
    void toggleFavourite (const juce::String& name);

    // A/B
    int abSlot() const noexcept { return abCurrent; }
    void switchAB (int slot);       // stores current into the active slot, loads the other
    void copyAB (int from, int to);
    bool slotFilled (int s) const { return abState[s].isValid(); }
    float slotLoudnessDb (int s) const { return abLoudness[s]; }
    void noteLoudness (float db) { abLoudness[abCurrent] = db; }
    /** Bounded (+-12 dB) wet trim on slot B so its measured wet loudness matches slot A. */
    bool matchLoudness (juce::String& message);
    std::unique_ptr<juce::XmlElement> abToXml() const;
    void abFromXml (const juce::XmlElement& x);
    std::function<void()> onChange;

private:
    juce::ValueTree captureState() const;
    void restoreState (const juce::ValueTree& v);
    void loadFavourites();
    void saveFavourites() const;
    PluginProcessor& proc;
    juce::Array<Entry> list;
    int current = 0;
    juce::String currentPresetName = "Held in the Afterglow";
    juce::StringArray favourites;
    juce::ValueTree abState[2];
    float abLoudness[2] { -120.0f, -120.0f };
    int abCurrent = 0;
};
} // namespace pa
