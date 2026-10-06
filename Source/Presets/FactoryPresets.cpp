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
        // ---- application presets (harmony off unless the name says otherwise): instantly usable starting points
        { "Vocal Plate", "Vocal", "Bright, short plate that sits behind a lead vocal; ducks while you sing.",
          { { "delayEnable", 0 }, { "reverbMode", 0 }, { "plDecay", 2.2f }, { "plPredelay", 30 }, { "plDiffusion", 85 }, { "plTone", 8500 },
            { "plMotion", 25 }, { "plLevel", -4 }, { "duckAmount", 30 }, { "duckRelease", 500 }, { "harmEnable", 0 }, { "depth", 0 }, { "mix", 40 } } },
        { "Slapback Room", "Vocal", "Single tape slap (110 ms) and a small room: classic rock'n'roll vocal or guitar.",
          { { "delayMode", 2 }, { "tpHeads", 0 }, { "tpTime", 110 }, { "tpFeedback", 12 }, { "tpDrive", 30 }, { "tpWow", 15 }, { "tpTone", 5000 },
            { "tpLevel", -6 }, { "reverbMode", 2 }, { "haSize", 22 }, { "haDecay", 0.9f }, { "haEarly", 55 }, { "haLevel", -10 },
            { "duckAmount", 0 }, { "harmEnable", 0 }, { "depth", 0 }, { "mix", 40 } } },
        { "Ducked Quarter Echo", "Vocal", "Tempo echo that steps aside while you sing and blooms in the gaps (dynamic-delay style).",
          { { "delayMode", 0 }, { "bbdSync", 1 }, { "bbdDiv", 8 }, { "bbdFeedback", 35 }, { "bbdTone", 4200 }, { "bbdAge", 15 }, { "bbdLevel", -4 },
            { "reverbMode", 0 }, { "plDecay", 1.8f }, { "plLevel", -12 }, { "duckAmount", 60 }, { "duckAttack", 8 }, { "duckRelease", 450 },
            { "harmEnable", 0 }, { "depth", 0 } } },
        { "Space Echo Dub", "Guitar", "Heads 1+3, warm saturated repeats with tape wobble into a modest hall. Push Feedback for dub throws.",
          { { "delayMode", 2 }, { "tpHeads", 5 }, { "tpTime", 280 }, { "tpFeedback", 68 }, { "tpDrive", 55 }, { "tpWow", 35 }, { "tpTone", 3400 },
            { "tpSpread", 70 }, { "tpLevel", -6 }, { "reverbMode", 2 }, { "haSize", 45 }, { "haDecay", 2.0f }, { "haLevel", -12 },
            { "duckAmount", 0 }, { "harmEnable", 0 }, { "depth", 0 } } },
        { "Runaway Tape", "Guitar", "Feedback past 100 %: the tape self-oscillates (bounded). Ride Feedback and Time for sirens and swells.",
          { { "delayMode", 2 }, { "tpHeads", 4 }, { "tpTime", 240 }, { "tpFeedback", 103 }, { "tpDrive", 65 }, { "tpWow", 30 }, { "tpTone", 3000 },
            { "tpLevel", -10 }, { "reverbEnable", 0 }, { "duckAmount", 0 }, { "harmEnable", 0 }, { "depth", 0 } } },
        { "Concert Hall", "Keys", "Natural large hall with clear early reflections and a slightly longer bass.",
          { { "delayEnable", 0 }, { "reverbMode", 2 }, { "haSize", 80 }, { "haDecay", 2.8f }, { "haPredelay", 24 }, { "haEarly", 35 },
            { "haTone", 7000 }, { "haLowRatio", 1.25f }, { "haLevel", -5 }, { "duckAmount", 0 }, { "harmEnable", 0 }, { "depth", 0 }, { "mix", 35 } } },
        { "Drum Room", "Drums", "Tight, reflective small room: glue for drums or a close acoustic source.",
          { { "delayEnable", 0 }, { "reverbMode", 2 }, { "haSize", 10 }, { "haDecay", 0.6f }, { "haPredelay", 0 }, { "haEarly", 75 },
            { "haDiffusion", 80 }, { "haTone", 9000 }, { "haLowRatio", 0.9f }, { "haMotion", 10 }, { "haLevel", -4 }, { "duckAmount", 0 },
            { "harmEnable", 0 }, { "depth", 0 }, { "mix", 30 } } },
        { "Snare Plate", "Drums", "Short bright plate with a little pre-delay, the classic snare and percussion sound.",
          { { "delayEnable", 0 }, { "reverbMode", 0 }, { "plDecay", 1.4f }, { "plPredelay", 12 }, { "plDiffusion", 92 }, { "plTone", 9500 },
            { "plLowRatio", 0.7f }, { "plLevel", -4 }, { "duckAmount", 0 }, { "harmEnable", 0 }, { "depth", 0 }, { "mix", 30 } } },
        { "Shimmer Cathedral", "Ambient", "Huge hall whose tail rises in octaves: pads, guitars and held vocal notes.",
          { { "delayEnable", 0 }, { "reverbMode", 2 }, { "haSize", 100 }, { "haDecay", 9 }, { "haPredelay", 40 }, { "haTone", 6000 },
            { "haMotion", 40 }, { "haLevel", -5 }, { "shimmer", 45 }, { "shimmerPitch", 0 }, { "duckAmount", 15 }, { "harmEnable", 0 }, { "depth", 0 } } },
        { "Octave Abyss", "Ambient", "Dark, endless wash with downward shimmer: drones and sub-heavy textures.",
          { { "delayEnable", 0 }, { "reverbMode", 1 }, { "waDecay", 50 }, { "waBloom", 85 }, { "waTone", 3200 }, { "waMotion", 40 },
            { "shimmer", 40 }, { "shimmerPitch", 4 }, { "duckAmount", 10 }, { "harmEnable", 0 }, { "depth", 0 } } },
        { "Shimmer Choir", "Showcase", "Octave shimmer hall vocoded to a stored Cmaj7: one sung note becomes a glowing chord.",
          { { "delayEnable", 0 }, { "reverbMode", 2 }, { "haSize", 85 }, { "haDecay", 7 }, { "haPredelay", 30 }, { "haTone", 6500 }, { "haLevel", -5 },
            { "shimmer", 35 }, { "shimmerPitch", 0 }, { "harmMethod", 1 }, { "harmEnable", 1 }, { "depth", 65 }, { "colour", 40 }, { "transition", 300 },
            { "noteSource", 1 }, { "chRoot", 0 }, { "chQuality", 5 }, { "duckAmount", 20 } } },
        { "Tape into Hall", "Mix", "Dotted-eighth tape echo feeding a medium hall (Delay > Reverb): a full, finished send.",
          { { "delayMode", 2 }, { "tpSync", 1 }, { "tpDiv", 6 }, { "tpHeads", 0 }, { "tpFeedback", 38 }, { "tpDrive", 30 }, { "tpTone", 4500 },
            { "tpLevel", -6 }, { "routing", 1 }, { "serialSend", 70 }, { "reverbMode", 2 }, { "haSize", 60 }, { "haDecay", 2.2f }, { "haLevel", -6 },
            { "duckAmount", 25 }, { "harmEnable", 0 }, { "depth", 0 } } },
        { "Synth Pad Bloom", "Keys", "Ping-pong BBD echoes melting into a 12 s wash: wide, soft and moving.",
          { { "delayMode", 0 }, { "bbdSync", 1 }, { "bbdDiv", 6 }, { "bbdStereo", 1 }, { "bbdFeedback", 48 }, { "bbdTone", 3000 }, { "bbdMotion", 30 },
            { "bbdLevel", -8 }, { "reverbMode", 1 }, { "waDecay", 12 }, { "waBloom", 60 }, { "waTone", 6000 }, { "waLevel", -6 },
            { "duckAmount", 10 }, { "harmEnable", 0 }, { "depth", 0 } } },
        { "Vocoded Tape Echo", "Showcase", "Tape echoes vocoded after the space: play MIDI chords and the repeats follow them.",
          { { "delayMode", 2 }, { "tpHeads", 3 }, { "tpTime", 330 }, { "tpFeedback", 55 }, { "tpDrive", 40 }, { "tpWow", 30 }, { "tpLevel", -6 },
            { "reverbMode", 2 }, { "haSize", 55 }, { "haDecay", 3 }, { "haLevel", -10 }, { "placement", 0 }, { "harmMethod", 1 }, { "harmEnable", 1 },
            { "depth", 80 }, { "clAttack", 6 }, { "clRelease", 120 }, { "duckAmount", 15 } } },
    };
    return list;
}
} // namespace pa
