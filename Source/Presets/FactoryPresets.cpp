#include "FactoryPresets.h"

namespace pa
{
bool isHostLocalParam (int index) noexcept
{
    return index == Timing || index == MidiChannel || index == InputSource || index == Tempo;
}

std::vector<ChordSnapshot> defaultSnapshots()
{
    return { { 0, 3, 1, 0, 0 }, { 8, 2, 0, 1, 0 }, { 5, 2, 1, 2, 0 }, { 3, 3, 0, 0, 0 },
             { 10, 2, 0, 1, 0 }, { 7, 2, 1, 1, 0 }, { 0, 3, 2, 0, 1 }, { 5, 2, 3, 0, 1 } };
}

void applyFactoryPreset (const FactoryPreset& fp, ParamSet& out)
{
    ParamSet def;
    for (int i = 0; i < kNumParams; ++i)
        if (! isHostLocalParam (i)) out[i] = def[i];
    for (const auto& kv : fp.values)
    {
        const int idx = paramIndexForId (kv.first);
        if (idx >= 0) out[idx] = paramSnap (idx, kv.second);
    }
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list = {
        { "Held in the Afterglow", "Showcase", "BBD + Wash, Classic vocoder after the space, held C minor. Sing, stop, change chords.",
          { { "delayMode", 0 }, { "reverbMode", 1 }, { "bbdSync", 1 }, { "bbdDiv", 8 }, { "bbdFeedback", 42 }, { "bbdTone", 3200 },
            { "bbdAge", 28 }, { "bbdLevel", -8 }, { "waDecay", 24 }, { "waBloom", 65 }, { "waTone", 4800 }, { "waMotion", 30 },
            { "waLevel", -6 }, { "harmMethod", 1 }, { "placement", 0 }, { "depth", 68 }, { "colour", 35 }, { "transition", 180 },
            { "duckAmount", 24 }, { "noteSource", 0 }, { "noNotePolicy", 0 }, { "latch", 1 }, { "chRoot", 0 }, { "chQuality", 1 } } },
        { "Velvet Plate", "Plate", "Dense plate with moderate Classic harmony; still clearly a plate at this depth.",
          { { "delayEnable", 0 }, { "reverbMode", 0 }, { "plDecay", 6 }, { "plDiffusion", 90 }, { "plTone", 7000 }, { "plLevel", -6 },
            { "harmMethod", 1 }, { "depth", 40 }, { "colour", 45 }, { "duckAmount", 15 } } },
        { "Plate Without Harmony", "Baseline", "Ordinary plate baseline (Harmony Off) for comparison.",
          { { "delayEnable", 0 }, { "reverbMode", 0 }, { "plDecay", 6 }, { "plDiffusion", 90 }, { "plTone", 7000 }, { "harmEnable", 0 }, { "depth", 0 } } },
        { "Endless Wash", "Baseline", "Long ordinary wash; press FREEZE to hold the texture.",
          { { "delayEnable", 0 }, { "reverbMode", 1 }, { "waDecay", 60 }, { "waBloom", 80 }, { "waMotion", 35 }, { "harmEnable", 0 }, { "depth", 0 },
            { "freezeTarget", 0 }, { "duckAmount", 10 } } },
        { "Frozen Choir", "Showcase", "Post-space vocoding of a long wash: freeze it, then play chords over the held texture.",
          { { "delayEnable", 0 }, { "reverbMode", 1 }, { "waDecay", 40 }, { "waBloom", 70 }, { "harmMethod", 1 }, { "depth", 90 }, { "colour", 40 },
            { "clBands", 2 }, { "transition", 400 }, { "noteRelease", 2500 }, { "freezeTarget", 0 }, { "clAttack", 25 }, { "clRelease", 350 } } },
        { "Classic / Matched", "Classic Comparison", "Matched comparison: Plate 10 s, C minor stored chord, full Depth, Classic vocoder.",
          { { "delayEnable", 0 }, { "reverbMode", 0 }, { "plDecay", 10 }, { "harmMethod", 1 }, { "depth", 100 }, { "noteSource", 1 },
            { "chRoot", 0 }, { "chQuality", 1 }, { "duckAmount", 0 }, { "wetTrim", 0 } } },
        { "Classic 48 / Matched", "Classic Comparison", "Same space and chord, Classic vocoder with 48 narrow bands (more intelligible, finer).",
          { { "delayEnable", 0 }, { "reverbMode", 0 }, { "plDecay", 10 }, { "harmMethod", 1 }, { "depth", 100 }, { "noteSource", 1 },
            { "clBands", 2 }, { "chRoot", 0 }, { "chQuality", 1 }, { "duckAmount", 0 }, { "wetTrim", 0 } } },
        { "Classic Bright / Matched", "Classic Comparison", "Same space and chord, Classic vocoder with the Bright carrier and an open Colour.",
          { { "delayEnable", 0 }, { "reverbMode", 0 }, { "plDecay", 10 }, { "harmMethod", 1 }, { "depth", 100 }, { "noteSource", 1 },
            { "clCarrier", 1 }, { "colour", 70 }, { "chRoot", 0 }, { "chQuality", 1 }, { "duckAmount", 0 }, { "wetTrim", 0 } } },
        { "Classic Hollow / Matched", "Classic Comparison", "Same space and chord, Classic vocoder with the Hollow (square) carrier: odd harmonics, woody.",
          { { "delayEnable", 0 }, { "reverbMode", 0 }, { "plDecay", 10 }, { "harmMethod", 1 }, { "depth", 100 }, { "noteSource", 1 },
            { "clCarrier", 2 }, { "chRoot", 0 }, { "chQuality", 1 }, { "duckAmount", 0 }, { "wetTrim", 0 } } },
        { "Echoes of Chords", "Delay", "Before Space: the played chord is imprinted, then repeats as BBD echoes (older chords keep echoing).",
          { { "delayMode", 0 }, { "bbdTime", 420 }, { "bbdFeedback", 58 }, { "bbdLevel", -6 }, { "reverbMode", 0 }, { "plDecay", 3 }, { "plLevel", -14 },
            { "placement", 1 }, { "harmMethod", 1 }, { "depth", 100 }, { "noNotePolicy", 1 }, { "noteRelease", 400 }, { "clAttack", 5 }, { "clRelease", 90 } } },
        { "Revoice the Echo", "Delay", "After Space: hold MIDI chords and the existing BBD repeats follow them.",
          { { "delayMode", 0 }, { "bbdTime", 450 }, { "bbdFeedback", 68 }, { "bbdLevel", -6 }, { "reverbEnable", 0 }, { "placement", 0 },
            { "harmMethod", 1 }, { "depth", 90 }, { "clAttack", 6 }, { "clRelease", 120 } } },
        { "Fifths in Orbit", "Delay", "Stable Interval delay: taps at 0, +7, +12 shifted once at the output.",
          { { "delayMode", 1 }, { "ivPitchMode", 0 }, { "ivShiftPlace", 0 }, { "ivTime", 480 }, { "ivFeedback", 45 }, { "ivTaps", 3 },
            { "reverbMode", 0 }, { "plDecay", 4 }, { "plLevel", -14 }, { "harmEnable", 0 }, { "depth", 0 } } },
        { "Climbing Repeats", "Delay", "Feedback Cascade: each repeat climbs another fifth (filtered and bounded).",
          { { "delayMode", 1 }, { "ivPitchMode", 0 }, { "ivShiftPlace", 1 }, { "ivTaps", 1 }, { "ivTap1Semi", 7 }, { "ivTap1Pan", 0 },
            { "ivTime", 380 }, { "ivFeedback", 72 }, { "ivTone", 7000 }, { "reverbEnable", 0 }, { "harmEnable", 0 }, { "depth", 0 } } },
        { "Clock Fragments", "Delay", "Clock behaviour: playback rate changes pitch and fragment duration together.",
          { { "delayMode", 1 }, { "ivPitchMode", 1 }, { "ivTaps", 2 }, { "ivTap1Semi", 12 }, { "ivTap2Semi", -12 }, { "ivTap2Level", -3 },
            { "ivGrain", 160 }, { "ivSmear", 40 }, { "ivTime", 600 }, { "ivFeedback", 40 }, { "reverbMode", 0 }, { "plDecay", 3 }, { "plLevel", -12 },
            { "harmEnable", 0 }, { "depth", 0 } } },
        { "Reverse Bloom", "Delay", "Reverse interval fragments feeding a long blooming Wash (Delay > Reverb).",
          { { "delayMode", 1 }, { "ivDirection", 1 }, { "ivGrain", 320 }, { "ivTaps", 2 }, { "ivTap2Semi", 12 }, { "ivTime", 700 }, { "ivFeedback", 30 },
            { "routing", 1 }, { "reverbMode", 1 }, { "waDecay", 30 }, { "waBloom", 90 }, { "harmMethod", 1 }, { "depth", 30 } } },
        { "Arpeggiated Air", "Showcase", "Arp notes articulated over a sustained (freezable) wash.",
          { { "delayEnable", 0 }, { "reverbMode", 1 }, { "waDecay", 30 }, { "noteSource", 3 }, { "arpRate", 3 }, { "arpGate", 45 }, { "arpOctaves", 2 },
            { "harmMethod", 1 }, { "depth", 90 }, { "noteAttack", 4 }, { "noteRelease", 260 }, { "clAttack", 3 }, { "clRelease", 70 },
            { "chQuality", 6 }, { "freezeTarget", 0 } } },
    };
    return list;
}
} // namespace pa
