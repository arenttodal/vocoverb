// Embedded, read-only factory presets (defaults + overrides). Shared by plugin, standalone and tools.
#pragma once

#include "../Core/Params.h"

#include <string>
#include <utility>
#include <vector>

namespace pa
{
struct ChordSnapshot { int root = 0, octave = 3, quality = 1, inversion = 0, spread = 0; };

struct FactoryPreset
{
    std::string name, category, description;
    std::vector<std::pair<std::string, float>> values;
};

const std::vector<FactoryPreset>& factoryPresets();
/** Resets to defaults then applies overrides. Parameters marked host-local (timing) are kept from `keep` if given. */
void applyFactoryPreset (const FactoryPreset& fp, ParamSet& out);
/** Default eight chord snapshots (C minor, A-flat, F minor, E-flat, B-flat, G minor, Csus2, Fsus4). */
std::vector<ChordSnapshot> defaultSnapshots();
/** Parameters that presets never override implicitly (device/host oriented). */
bool isHostLocalParam (int index) noexcept;
} // namespace pa
