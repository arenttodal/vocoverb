// pa_render: offline renderer for audition comparisons and factory preset export (no JUCE dependency).
//   pa_render --out DIR             render the comparison WAV set + manifest.md
//   pa_render --export-presets DIR  write every factory preset as a .papreset JSON file
#include "Core/Analysis.h"
#include "Core/Engine.h"
#include "Core/Fixtures.h"
#include "Core/Sequences.h"
#include "Presets/FactoryPresets.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace pa;
namespace fs = std::filesystem;

namespace
{
void writeWav16 (const fs::path& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    std::ofstream f (path, std::ios::binary);
    const uint32_t n = (uint32_t) L.size();
    const uint32_t dataBytes = n * 4;
    auto u32 = [&f] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&f] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); u32 (36 + dataBytes); f.write ("WAVE", 4);
    f.write ("fmt ", 4); u32 (16); u16 (1); u16 (2); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 4); u16 (4); u16 (16);
    f.write ("data", 4); u32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        const float a = std::fmax (-1.0f, std::fmin (1.0f, L[i])), b = std::fmax (-1.0f, std::fmin (1.0f, R[i]));
        u16 ((uint16_t) (int16_t) std::lrint (a * 32767.0f));
        u16 ((uint16_t) (int16_t) std::lrint (b * 32767.0f));
    }
}

std::string jsonEscape (const std::string& s)
{
    std::string o;
    for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
    return o;
}

const FactoryPreset* findPreset (const std::string& name)
{
    for (const auto& p : factoryPresets()) if (p.name == name) return &p;
    return nullptr;
}

struct RenderJob
{
    std::string file, preset, description;
    Fixture fixture = Fixture::HarmonicTone;
    int experiment = 0;          // index in builtInExperiments(), -1 = none
    double seconds = 10.0;
    std::vector<std::pair<int, float>> overrides;
};

double renderOne (const RenderJob& j, const fs::path& dir, double sr, std::string& stats)
{
    ParamSet p;
    if (auto* fp = findPreset (j.preset)) applyFactoryPreset (*fp, p);
    p[Timing] = 0;
    for (auto& [i, v] : j.overrides) p[i] = v;
    std::vector<float> fl, fr;
    generateFixture (j.fixture, sr, fl, fr);
    const int total = (int) (j.seconds * sr);
    fl.resize ((size_t) total, 0.0f); fr.resize ((size_t) total, 0.0f);
    std::vector<TimedEvent> events;
    if (j.experiment >= 0) events = builtInExperiments()[(size_t) j.experiment].events;
    auto eng = std::make_unique<Engine>();
    eng->prepare (sr, 512);
    std::vector<float> L ((size_t) total), R ((size_t) total);
    std::vector<MidiEvent> ev (256);
    TransportInfo tp;
    for (int pos = 0; pos < total; pos += 512)
    {
        const int n = std::min (512, total - pos);
        const int ne = eventsInWindow (events, pos / sr, (pos + n) / sr, sr, ev.data(), (int) ev.size(), Origin::Sequence);
        eng->process (p, tp, fl.data() + pos, fr.data() + pos, L.data() + pos, R.data() + pos, n, ev.data(), ne);
    }
    writeWav16 (dir / j.file, L, R, (int) sr);
    const double r = rms (L.data(), total), pk = std::max (peakAbs (L.data(), total), peakAbs (R.data(), total));
    char buf[160];
    std::snprintf (buf, sizeof (buf), "RMS %.1f dBFS, peak %.1f dBFS", 20.0 * std::log10 (r + 1e-12), 20.0 * std::log10 (pk + 1e-12));
    stats = buf;
    return pk;
}

int exportPresets (const fs::path& dir)
{
    fs::create_directories (dir);
    for (const auto& fp : factoryPresets())
    {
        ParamSet p;
        applyFactoryPreset (fp, p);
        std::ostringstream o;
        o << "{\n  \"product\": \"Playable Ambience\",\n  \"schema\": 1,\n  \"paramVersion\": " << kParamVersion << ",\n  \"name\": \"" << jsonEscape (fp.name)
          << "\",\n  \"category\": \"" << jsonEscape (fp.category) << "\",\n  \"description\": \"" << jsonEscape (fp.description) << "\",\n  \"params\": {\n";
        bool first = true;
        for (int i = 0; i < kNumParams; ++i)
        {
            if (i == Freeze || isHostLocalParam (i)) continue;
            o << (first ? "" : ",\n") << "    \"" << paramInfo (i).id << "\": " << p[i];
            first = false;
        }
        o << "\n  },\n  \"chordSnapshots\": [";
        auto snaps = defaultSnapshots();
        for (size_t k = 0; k < snaps.size(); ++k)
            o << (k ? ", " : "") << "{\"root\": " << snaps[k].root << ", \"octave\": " << snaps[k].octave << ", \"quality\": " << snaps[k].quality
              << ", \"inversion\": " << snaps[k].inversion << ", \"spread\": " << snaps[k].spread << "}";
        o << "]\n}\n";
        std::string fname = fp.name;
        for (auto& c : fname) if (c == '/' || c == '\\' || c == ':') c = '-';
        std::ofstream (dir / (fname + ".papreset")) << o.str();
    }
    std::printf ("exported %zu presets to %s\n", factoryPresets().size(), dir.string().c_str());
    return 0;
}
} // namespace

int main (int argc, char** argv)
{
    fs::path out, presetsOut;
    double sr = 48000.0;
    for (int i = 1; i < argc; ++i)
    {
        if (! std::strcmp (argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if (! std::strcmp (argv[i], "--export-presets") && i + 1 < argc) presetsOut = argv[++i];
        else if (! std::strcmp (argv[i], "--sr") && i + 1 < argc) sr = std::atof (argv[++i]);
    }
    if (! presetsOut.empty()) return exportPresets (presetsOut);
    if (out.empty()) { std::printf ("usage: pa_render --out DIR | --export-presets DIR\n"); return 2; }
    fs::create_directories (out);
    const int cMinor = 0, freezeRevoice = 1, held = 4;
    std::vector<RenderJob> jobs = {
        { "01-sing-stop-revoice.wav", "Held in the Afterglow", "Defining demo: synthetic sung phrase ends ~3.3 s; C minor > A-flat > F minor revoice the remaining BBD + Wash tail (Classic, After Space).", Fixture::HarmonicTone, cMinor, 11.0, {} },
        { "02-plate-baseline.wav", "Plate Without Harmony", "Ordinary plate (Harmony Off) for comparison.", Fixture::HarmonicTone, cMinor, 9.0, {} },
        { "03-velvet-plate.wav", "Velvet Plate", "Same plate with moderate (40%) Classic harmony.", Fixture::HarmonicTone, cMinor, 9.0, {} },
        { "04-method-classic.wav", "Classic / Matched", "Matched comparison: Classic filter-bank vocoder, breath/noise source.", Fixture::BreathNoise, cMinor, 9.0, {} },
        { "05-classic-48-bands.wav", "Classic 48 / Matched", "Matched comparison: Classic with 48 bands.", Fixture::BreathNoise, cMinor, 9.0, {} },
        { "06-classic-bright.wav", "Classic Bright / Matched", "Matched comparison: Classic with the Bright carrier.", Fixture::BreathNoise, cMinor, 9.0, {} },
        { "07-classic-hollow.wav", "Classic Hollow / Matched", "Matched comparison: Classic with the Hollow carrier, breath/noise source.", Fixture::BreathNoise, cMinor, 9.0, {} },
        { "08-before-space-echoes.wav", "Echoes of Chords", "Before Space: chords imprinted then echoed (older chords keep echoing).", Fixture::BreathNoise, cMinor, 10.0, {} },
        { "09-after-space-echoes.wav", "Revoice the Echo", "After Space: the existing BBD repeats follow the new chords.", Fixture::BreathNoise, cMinor, 10.0, {} },
        { "10-freeze-revoice.wav", "Frozen Choir", "Freeze at 3.4 s, then four chord changes over the frozen wash.", Fixture::BreathNoise, freezeRevoice, 12.0, {} },
        { "11-fifths-in-orbit.wav", "Fifths in Orbit", "Stable Interval delay, output shifts 0/+7/+12.", Fixture::PluckedSequence, -1, 9.0, {} },
        { "12-climbing-repeats.wav", "Climbing Repeats", "Feedback Cascade: each repeat climbs a fifth (bounded).", Fixture::PluckedSequence, -1, 9.0, {} },
        { "13-clock-fragments.wav", "Clock Fragments", "Clock mode: rate couples pitch and fragment duration.", Fixture::PluckedSequence, -1, 9.0, {} },
        { "14-reverse-bloom.wav", "Reverse Bloom", "Reverse interval fragments into a blooming Wash.", Fixture::BowedTexture, -1, 10.0, {} },
        { "15-arpeggiated-air.wav", "Arpeggiated Air", "Arpeggiator (free 120 BPM grid) over a sustained wash; held C minor 7.", Fixture::BowedTexture, held, 9.0, {} },
    };
    std::ostringstream man;
    man << "# Audio examples (Playable Ambience 0.1.0)\n\nRendered offline by `pa_render` with the shipping DSP engine at " << (int) sr
        << " Hz, 16-bit stereo. Sources are clearly synthetic fixtures (no recordings). Timing policy Live (dry immediate).\n"
           "Levels are not loudness-matched unless the preset says so; compare by ear, then use A/B loudness match in the app.\n\n"
           "| File | Preset | What to listen for | Level |\n|---|---|---|---|\n";
    int failures = 0;
    for (const auto& j : jobs)
    {
        std::string stats;
        const double pk = renderOne (j, out, sr, stats);
        if (! std::isfinite (pk) || pk > 1.0) ++failures;
        man << "| " << j.file << " | " << j.preset << " | " << j.description << " | " << stats << " |\n";
        std::printf ("%-32s %s\n", j.file.c_str(), stats.c_str());
    }
    std::ofstream (out / "manifest.md") << man.str();
    return failures == 0 ? 0 : 1;
}
