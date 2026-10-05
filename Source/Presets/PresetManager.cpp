#include "PresetManager.h"

#include "BuildInfo.h"
#include "Plugin/PluginProcessor.h"

namespace pa
{
PresetManager::PresetManager (PluginProcessor& p) : proc (p)
{
    loadFavourites();
    rescan();
}

juce::File PresetManager::userPresetDirectory()
{
#if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support").getChildFile ("Playable Ambience").getChildFile ("Presets");
#else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Playable Ambience").getChildFile ("Presets");
#endif
}

void PresetManager::rescan()
{
    list.clear();
    const auto& fp = factoryPresets();
    for (int i = 0; i < (int) fp.size(); ++i)
        list.add ({ fp[(size_t) i].name, fp[(size_t) i].category, fp[(size_t) i].description, true, i, {} });
    auto dir = userPresetDirectory();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.papreset");
        files.sort();
        for (auto& f : files)
        {
            auto v = juce::JSON::parse (f);
            if (! v.isObject()) continue;
            list.add ({ v.getProperty ("name", f.getFileNameWithoutExtension()).toString(), v.getProperty ("category", "User").toString(),
                        v.getProperty ("description", "").toString(), false, -1, f });
        }
    }
}

const juce::Array<PresetManager::Entry>& PresetManager::entries() { return list; }

void PresetManager::loadFactory (int index, bool notify)
{
    const auto& fp = factoryPresets();
    if (index < 0 || index >= (int) fp.size()) return;
    ParamSet ps = proc.currentParams();
    applyFactoryPreset (fp[(size_t) index], ps);
    proc.applyParams (ps, false);
    proc.snapshots = defaultSnapshots();
    current = index;
    currentPresetName = fp[(size_t) index].name;
    if (notify && onChange) onChange();
}

bool PresetManager::loadEntry (int index)
{
    if (index < 0 || index >= list.size()) return false;
    const auto& e = list.getReference (index);
    if (e.factory) { loadFactory (e.factoryIndex); current = index; return true; }
    juce::String err;
    if (! loadFile (e.file, err)) return false;
    current = index;
    return true;
}

void PresetManager::step (int delta)
{
    if (list.isEmpty()) return;
    int i = (current + delta) % list.size();
    if (i < 0) i += list.size();
    loadEntry (i);
}

juce::var PresetManager::toJson (const juce::String& name) const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("product", "Playable Ambience");
    o->setProperty ("schema", kPresetSchema);
    o->setProperty ("paramVersion", kParamVersion);
    o->setProperty ("appVersion", PA_BUILD_VERSION);
    o->setProperty ("name", name);
    o->setProperty ("category", "User");
    auto* pv = new juce::DynamicObject();
    for (int i = 0; i < kNumParams; ++i)
    {
        if (i == Freeze) continue; // captured/frozen audio and freeze engagement are not stored
        pv->setProperty (paramInfo (i).id, (double) proc.value (i));
    }
    o->setProperty ("params", juce::var (pv));
    juce::Array<juce::var> snaps;
    for (const auto& s : proc.snapshots)
    {
        auto* so = new juce::DynamicObject();
        so->setProperty ("root", s.root); so->setProperty ("octave", s.octave); so->setProperty ("quality", s.quality);
        so->setProperty ("inversion", s.inversion); so->setProperty ("spread", s.spread);
        snaps.add (juce::var (so));
    }
    o->setProperty ("chordSnapshots", snaps);
    return juce::var (o);
}

bool PresetManager::fromJson (const juce::var& v, juce::String& error)
{
    if (! v.isObject()) { error = "Not a preset file"; return false; }
    const int schema = (int) v.getProperty ("schema", 0);
    if (schema < 1 || schema > kPresetSchema) { error = "Unsupported preset schema " + juce::String (schema); return false; }
    // schema 1: params by stable id; unknown ids ignored, missing ids take defaults (host-local params kept)
    ParamSet ps = proc.currentParams();
    ParamSet def;
    for (int i = 0; i < kNumParams; ++i) if (! isHostLocalParam (i)) ps[i] = def[i];
    const auto pv = v.getProperty ("params", {});
    if (auto* obj = pv.getDynamicObject())
        for (auto& kv : obj->getProperties())
        {
            const int idx = paramIndexForId (kv.name.toString().toStdString());
            if (idx >= 0) ps[idx] = paramSnap (idx, (float) (double) kv.value);
        }
    proc.applyParams (ps, false);
    const auto snaps = v.getProperty ("chordSnapshots", {});
    if (auto* arr = snaps.getArray())
    {
        proc.snapshots.clear();
        for (auto& s : *arr)
            proc.snapshots.push_back ({ (int) s.getProperty ("root", 0), (int) s.getProperty ("octave", 3), (int) s.getProperty ("quality", 1),
                                        (int) s.getProperty ("inversion", 0), (int) s.getProperty ("spread", 0) });
        while (proc.snapshots.size() < 8) proc.snapshots.push_back ({});
    }
    currentPresetName = v.getProperty ("name", "Untitled").toString();
    if (onChange) onChange();
    return true;
}

bool PresetManager::loadFile (const juce::File& f, juce::String& error)
{
    if (! f.existsAsFile()) { error = "File not found"; return false; }
    if (f.getSize() > 1024 * 1024) { error = "Preset file too large"; return false; }
    return fromJson (juce::JSON::parse (f), error);
}

bool PresetManager::saveUser (const juce::String& nameIn, Collision policy, juce::String& error)
{
    auto name = nameIn.trim();
    if (name.isEmpty()) { error = "Empty name"; return false; }
    auto dir = userPresetDirectory();
    if (! dir.createDirectory()) { error = "Cannot create " + dir.getFullPathName(); return false; }
    auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".papreset");
    if (file.existsAsFile())
    {
        if (policy == Collision::Ask) { error = "exists"; return false; }
        if (policy == Collision::KeepBoth)
        {
            file = file.getNonexistentSibling (true);
            name = file.getFileNameWithoutExtension();
        }
    }
    juce::TemporaryFile tmp (file);
    if (! tmp.getFile().replaceWithText (juce::JSON::toString (toJson (name), false))) { error = "Write failed"; return false; }
    if (! tmp.overwriteTargetFileWithTemporary()) { error = "Atomic replace failed"; return false; }
    currentPresetName = name;
    rescan();
    for (int i = 0; i < list.size(); ++i) if (! list[i].factory && list[i].file == file) current = i;
    if (onChange) onChange();
    return true;
}

void PresetManager::loadFavourites()
{
    auto f = userPresetDirectory().getSiblingFile ("favourites.json");
    auto v = juce::JSON::parse (f);
    favourites.clear();
    if (auto* a = v.getArray()) for (auto& s : *a) favourites.add (s.toString());
}

void PresetManager::saveFavourites() const
{
    auto f = userPresetDirectory().getSiblingFile ("favourites.json");
    f.getParentDirectory().createDirectory();
    juce::Array<juce::var> a;
    for (auto& s : favourites) a.add (s);
    juce::TemporaryFile tmp (f);
    if (tmp.getFile().replaceWithText (juce::JSON::toString (juce::var (a)))) tmp.overwriteTargetFileWithTemporary();
}

void PresetManager::toggleFavourite (const juce::String& name)
{
    if (favourites.contains (name)) favourites.removeString (name); else favourites.add (name);
    saveFavourites();
    if (onChange) onChange();
}

// ------------------------------------------------------------------------------------------- A/B
juce::ValueTree PresetManager::captureState() const
{
    juce::ValueTree v ("SLOT");
    for (int i = 0; i < kNumParams; ++i) if (i != Freeze) v.setProperty (paramInfo (i).id, (double) proc.value (i), nullptr);
    v.setProperty ("presetName", currentPresetName, nullptr);
    juce::String snaps;
    for (auto& s : proc.snapshots) snaps << s.root << "," << s.octave << "," << s.quality << "," << s.inversion << "," << s.spread << ";";
    v.setProperty ("snapshots", snaps, nullptr);
    return v;
}

void PresetManager::restoreState (const juce::ValueTree& v)
{
    ParamSet ps = proc.currentParams();
    for (int i = 0; i < kNumParams; ++i)
        if (v.hasProperty (paramInfo (i).id)) ps[i] = paramSnap (i, (float) (double) v.getProperty (paramInfo (i).id));
    proc.applyParams (ps, true); // A/B includes the timing policy
    currentPresetName = v.getProperty ("presetName", currentPresetName).toString();
    auto parts = juce::StringArray::fromTokens (v.getProperty ("snapshots").toString(), ";", "");
    std::vector<ChordSnapshot> snaps;
    for (auto& p : parts)
    {
        auto f = juce::StringArray::fromTokens (p, ",", "");
        if (f.size() == 5) snaps.push_back ({ f[0].getIntValue(), f[1].getIntValue(), f[2].getIntValue(), f[3].getIntValue(), f[4].getIntValue() });
    }
    if (snaps.size() >= 8) proc.snapshots = snaps;
}

void PresetManager::switchAB (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == abCurrent) return;
    abState[abCurrent] = captureState();        // previous snapshot stays until replacement succeeds
    if (abState[slot].isValid()) restoreState (abState[slot]);
    else abState[slot] = abState[abCurrent].createCopy(); // empty slot starts as a copy
    abCurrent = slot;
    if (proc.clearTailOnAB) proc.tailKill();
    if (onChange) onChange();
}

void PresetManager::copyAB (int from, int to)
{
    from = juce::jlimit (0, 1, from); to = juce::jlimit (0, 1, to);
    if (from == to) return;
    const auto src = from == abCurrent ? captureState() : abState[from];
    if (! src.isValid()) return;
    abState[to] = src.createCopy();
    if (to == abCurrent) restoreState (abState[to]);
    if (onChange) onChange();
}

bool PresetManager::matchLoudness (juce::String& message)
{
    if (abLoudness[0] < -100.0f || abLoudness[1] < -100.0f) { message = "Play audio through both A and B first."; return false; }
    const float diff = juce::jlimit (-12.0f, 12.0f, abLoudness[0] - abLoudness[1]);
    if (abCurrent == 1)
    {
        proc.setValue (WetTrim, juce::jlimit (-12.0f, 12.0f, proc.value (WetTrim) + diff));
    }
    else
    {
        if (! abState[1].isValid()) { message = "Slot B is empty."; return false; }
        const float cur = (float) (double) abState[1].getProperty (paramInfo (WetTrim).id, 0.0);
        abState[1].setProperty (paramInfo (WetTrim).id, juce::jlimit (-12.0f, 12.0f, cur + diff), nullptr);
    }
    abLoudness[1] += diff;
    message = "B wet trim adjusted by " + juce::String (diff, 1) + " dB (wet only, bounded +/-12 dB).";
    return true;
}

std::unique_ptr<juce::XmlElement> PresetManager::abToXml() const
{
    auto x = std::make_unique<juce::XmlElement> ("AB");
    x->setAttribute ("current", abCurrent);
    for (int s = 0; s < 2; ++s)
        if (abState[s].isValid())
            if (auto e = abState[s].createXml()) { e->setAttribute ("slot", s); x->addChildElement (e.release()); }
    return x;
}

void PresetManager::abFromXml (const juce::XmlElement& x)
{
    abCurrent = juce::jlimit (0, 1, x.getIntAttribute ("current", 0));
    for (auto* e : x.getChildIterator())
    {
        const int s = juce::jlimit (0, 1, e->getIntAttribute ("slot", 0));
        abState[s] = juce::ValueTree::fromXml (*e);
        abState[s].removeProperty ("slot", nullptr);
    }
}
} // namespace pa
