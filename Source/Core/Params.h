// Playable Ambience — parameter table. IDs are stable host automation identifiers (version 1).
// Never rename an id; add new parameters at the end of a group and bump kParamVersion if semantics change.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace pa
{
enum class Kind : uint8_t { Float, Int, Bool, Choice };

struct ParamInfo
{
    const char* id;
    const char* name;
    Kind kind;
    float minV, maxV, def;
    float centre;        // skew mid-point for UI/host mapping (0 = linear)
    const char* unit;    // "dB", "ms", "Hz", "%", "s", "st", "ct", "note" ...
    const char* choices; // '|' separated for Choice
    const char* group;   // "Mix", "Harmony", "Delay", "Reverb", "MIDI", "Timing", "Chord", "Intervals", "Arp", ...
    const char* help;
};

#define PA_DIVS "1/32|1/16T|1/16|1/16D|1/8T|1/8|1/8D|1/4T|1/4|1/4D|1/2T|1/2|1/2D|1 bar|2 bars"
#define PA_KEYS "C|C#|D|Eb|E|F|F#|G|Ab|A|Bb|B"

// X(ENUM, id, name, kind, min, max, default, centre, unit, choices, group, help)
#define PA_PARAMS(X) \
 X(DryLevel, "dryLevel", "Dry Level", Float, -60, 6, 0, -12, "dB", "", "Mix", "Unprocessed source level. Only this gain (and Studio alignment delay) touches the dry signal. -60 = off.") \
 X(WetLevel, "wetLevel", "Wet Level", Float, -60, 6, -9, -12, "dB", "", "Mix", "Final ambience (wet return) level (Settings > Audio). It never affects the dry signal; the header DRY / WET knob blends dry against wet.") \
 X(WetOnly, "wetOnly", "Wet Only", Bool, 0, 1, 0, 0, "", "", "Mix", "Removes the dry signal completely. Use on return/send tracks or when the source track stays audible.") \
 X(WetTrim, "wetTrim", "Wet Trim", Float, -12, 12, 0, 0, "dB", "", "Mix", "Wet-only trim used by the A/B loudness match. Bounded to +/-12 dB.") \
 X(InputSource, "inputSource", "Input Source", Choice, 0, 2, 0, 0, "", "Main + Sidechain|Main Only|Sidechain Only", "Mix", "Which input buses feed the effect. Logic MIDI-controlled AU: audio arrives on the sidechain.") \
 X(Routing, "routing", "Routing", Choice, 0, 2, 0, 0, "", "Parallel|Delay > Reverb|Reverb > Delay", "Mix", "Parallel: both engines hear the input. Series: the first engine's wet output feeds the second.") \
 X(Placement, "placement", "Placement", Choice, 0, 1, 0, 0, "", "After Space|Before Space", "Harmony", "After Space vocodes the ambience itself (existing tails follow new chords). Before Space imprints chords that then echo/reverberate.") \
 X(SerialSend, "serialSend", "Serial Send", Float, 0, 100, 100, 0, "%", "", "Mix", "Series routing only: how much of the first engine feeds the second, independent of the first engine's direct level.") \
 X(ApplyHarmonyTo, "applyHarmonyTo", "Apply Harmony To", Choice, 0, 2, 0, 0, "", "Both|Delay Only|Reverb Only", "Harmony", "Parallel routing only: which branch is harmonized. The other branch stays ordinary ambience.") \
 X(ClearTailOnChange, "clearTailOnChange", "Clear Tail on Change", Bool, 0, 1, 0, 0, "", "", "Mix", "When on, switching engine modes cuts the old tail quickly (exact comparisons). When off, the old tail rings out.") \
 X(DelayEnable, "delayEnable", "Delay Enable", Bool, 0, 1, 1, 0, "", "", "Delay", "Turns the delay engine on or off (processing, not just display).") \
 X(ReverbEnable, "reverbEnable", "Reverb Enable", Bool, 0, 1, 1, 0, "", "", "Reverb", "Turns the reverb engine on or off (processing, not just display).") \
 X(DelayMode, "delayMode", "Delay Mode", Choice, 0, 2, 0, 0, "", "BBD|Interval|Tape", "Delay", "BBD: warm darkening echoes. Interval: pitch-shaped taps. Tape: three-head tape echo.") \
 X(ReverbMode, "reverbMode", "Reverb Mode", Choice, 0, 2, 1, 0, "", "Plate|Wash|Hall", "Reverb", "Plate: dense, immediate. Wash: slow bloom, very long modulated tail. Hall: rooms to large halls with early reflections.") \
 X(Width, "width", "Width", Float, 0, 150, 100, 0, "%", "", "Mix", "Wet stereo width (mid/side). 0% = mono wet; mono fold-down stays intact.") \
 X(WetLowCut, "wetLowCut", "Wet Low Cut", Float, 20, 1000, 100, 150, "Hz", "", "Mix", "High-pass on the wet branch only.") \
 X(WetHighCut, "wetHighCut", "Wet High Cut", Float, 1000, 20000, 12000, 5000, "Hz", "", "Mix", "Low-pass on the wet branch only.") \
 X(DuckAmount, "duckAmount", "Duck", Float, 0, 100, 20, 0, "%", "", "Mix", "Lowers the wet output while the dry input is loud, so ambience blooms after the phrase. 100% = about 24 dB.") \
 X(DuckAttack, "duckAttack", "Duck Attack", Float, 1, 200, 10, 30, "ms", "", "Mix", "How fast ducking engages.") \
 X(DuckRelease, "duckRelease", "Duck Release", Float, 20, 3000, 350, 400, "ms", "", "Mix", "How fast the wet returns after the source stops.") \
 X(Timing, "timing", "Timing", Choice, 0, 1, 1, 0, "", "Live|Studio", "Timing", "Live: dry is immediate, wet carries extra processing delay. Studio: dry and wet aligned, fixed latency reported to the host.") \
 X(Quality, "quality", "Quality", Choice, 0, 2, 1, 0, "", "Eco|Standard|High", "Timing", "Eco/Standard/High: FFT size 1024/2048/4096, resonator partial cap 6/12/16, Wash lines 8/16/16, shift window 40/60/80 ms, Classic filter order 2/4/4. Changing it clears the wet tail.") \
 X(HarmMethod, "harmMethod", "Harmony Method", Choice, 0, 4, 1, 0, "", "Off|Classic|FFT|Resonator|Shift", "Harmony", "Legacy selector kept for saved sessions and automation: this build always uses Classic. Off maps to Harmony Enable off; FFT/Resonator/Shift load as Classic. Former help: Off: ordinary ambience. Classic: filter-bank vocoder. FFT: STFT vocoder. Resonator: tuned resonances excited by the wet audio. Shift: relative transposition.") \
 X(Depth, "depth", "Harmony Depth", Float, 0, 100, 60, 0, "%", "", "Harmony", "Blend between ordinary wet (0%) and harmonized wet (100%). Linear blend.") \
 X(Colour, "colour", "Colour", Float, 0, 100, 35, 0, "%", "", "Harmony", "Classic/FFT: dark-to-bright carrier tilt. Resonator: partial brightness. Shift: high-frequency tone of the shifted voices.") \
 X(Transition, "transition", "Transition", Float, 0, 2000, 120, 300, "ms", "", "Harmony", "Crossfade or glide time when one chord replaces another. Independent of room decay.") \
 X(TransitionMode, "transitionMode", "Transition Mode", Choice, 0, 1, 0, 0, "", "Crossfade|Glide", "Harmony", "Crossfade: old chord fades while new fades in. Glide: voices slide to the nearest new note.") \
 X(NoteAttack, "noteAttack", "Note Attack", Float, 1, 500, 15, 60, "ms", "", "Harmony", "Carrier/resonator excitation attack for new notes.") \
 X(NoteRelease, "noteRelease", "Note Release", Float, 5, 10000, 1200, 1000, "ms", "", "Harmony", "How notes fade after key release. Not the room decay.") \
 X(Polyphony, "polyphony", "Polyphony", Int, 1, 6, 6, 0, "", "", "Harmony", "Maximum harmony voices. Extra notes steal a quiet releasing voice, otherwise the oldest.") \
 X(NoNotePolicy, "noNotePolicy", "No-Note Policy", Choice, 0, 2, 0, 0, "", "Hold Last|Release|Ambient", "Harmony", "What happens when no keys are held. Hold Last keeps the last chord; Release lets voices fade; Ambient returns to ordinary ambience.") \
 X(Latch, "latch", "Latch", Bool, 0, 1, 0, 0, "", "", "Harmony", "Holds played notes after key-up; the next chord (after all keys are up) replaces them.") \
 X(NoteSource, "noteSource", "Note Source", Choice, 0, 3, 0, 0, "", "MIDI|Chord|Intervals|Arp", "Harmony", "Where harmony notes come from: live/DAW MIDI and the on-screen keys, a stored chord, an interval set, or the arpeggiator.") \
 X(RefTuning, "refTuning", "Reference Tuning", Float, 400, 480, 440, 0, "Hz", "", "MIDI", "Frequency of A4 for all MIDI pitches.") \
 X(MidiChannel, "midiChannel", "MIDI Channel", Int, 0, 16, 0, 0, "", "", "MIDI", "0 = Omni. On-screen keys are always accepted.") \
 X(PitchBendRange, "pitchBendRange", "Pitch Bend Range", Choice, 0, 1, 0, 0, "", "2 st|12 st", "MIDI", "Global pitch bend range for harmony voices.") \
 X(ModWheelTarget, "modWheelTarget", "Mod Wheel", Choice, 0, 2, 0, 0, "", "Harmony Depth|Motion|None", "MIDI", "Mod wheel adds to this target (bounded).") \
 X(ChordWindow, "chordWindow", "Chord Window", Float, 5, 100, 30, 0, "ms", "", "MIDI", "Notes arriving within this window after a latch replacement join the same new chord.") \
 X(Freeze, "freeze", "Freeze", Bool, 0, 1, 0, 0, "", "", "Freeze", "Sustains the existing wet texture (it never creates sound on its own). Harmony can keep changing over it.") \
 X(FreezeTarget, "freezeTarget", "Freeze Target", Choice, 0, 2, 0, 0, "", "Reverb|Delay|Both", "Freeze", "Reverb: near-lossless tank hold. Delay: crossfaded loop of the recent delay output.") \
 X(FreezeOverdub, "freezeOverdub", "Freeze Overdub", Float, 0, 100, 0, 0, "%", "", "Freeze", "Lets new input enter a frozen engine at a bounded level.") \
 X(ClBands, "clBands", "Classic Bands", Choice, 0, 2, 1, 0, "", "24|32|48", "Classic", "Number of logarithmic analysis/synthesis bands.") \
 X(ClAttack, "clAttack", "Envelope Attack", Float, 1, 200, 12, 30, "ms", "", "Classic", "Band envelope attack. Short for delays/arp articulation, longer for smooth ambience.") \
 X(ClRelease, "clRelease", "Envelope Release", Float, 10, 2000, 220, 250, "ms", "", "Classic", "Band envelope release.") \
 X(ClCarrier, "clCarrier", "Carrier Shape", Choice, 0, 2, 0, 0, "", "Soft|Bright|Hollow", "Classic", "Soft: saw. Bright: saw + narrow pulse. Hollow: square (odd harmonics). Shared by the FFT vocoder.") \
 X(ClDetune, "clDetune", "Carrier Detune", Float, 0, 50, 0, 0, "ct", "", "Classic", "Adds a second detuned oscillator per note (0 = one oscillator per note).") \
 X(ClFormant, "clFormant", "Formant Shift", Float, -12, 12, 0, 0, "st", "", "Classic", "Moves the analysed envelope up/down the synthesis bands; played notes stay the same.") \
 X(ClStereoLink, "clStereoLink", "Stereo Link", Bool, 0, 1, 0, 0, "", "", "Classic", "Off: separate left/right envelopes keep spatial movement. On: linked mono envelope.") \
 X(ClNoise, "clNoise", "Sibilance Preserve", Float, 0, 100, 0, 0, "%", "", "Classic", "Adds high-passed (>4 kHz) wet noise for consonants. Documented exception to full-Depth replacement.") \
 X(FftSmooth, "fftSmooth", "Spectral Smoothing", Float, 0, 100, 45, 0, "%", "", "FFT", "Frequency smoothing of the analysed envelope (1/12 to 2/3 octave). Low values keep more of the original pitch comb.") \
 X(FftPersist, "fftPersist", "Envelope Persistence", Float, 0, 100, 30, 0, "%", "", "FFT", "Temporal hold of the spectral envelope between frames.") \
 X(FftFormant, "fftFormant", "FFT Formant Shift", Float, -12, 12, 0, 0, "st", "", "FFT", "Shifts the spectral envelope; carrier pitches stay fixed.") \
 X(FftStereoLink, "fftStereoLink", "FFT Stereo Link", Bool, 0, 1, 0, 0, "", "", "FFT", "Linked vs separate left/right spectral envelopes.") \
 X(RsHarmonics, "rsHarmonics", "Harmonics", Int, 1, 16, 10, 0, "", "", "Resonator", "Partials per note (capped by Quality: Eco 6, Standard 12, High 16). Partials near Nyquist are dropped.") \
 X(RsDecay, "rsDecay", "Resonance Decay", Float, 0.05f, 10, 1.5f, 1, "s", "", "Resonator", "Ring time of the fundamental resonance.") \
 X(RsDamping, "rsDamping", "Damping", Float, 0, 100, 40, 0, "%", "", "Resonator", "Higher partials decay faster as damping rises.") \
 X(RsSpread, "rsSpread", "Spread", Float, 0, 100, 50, 0, "%", "", "Resonator", "Alternates partials across the stereo field.") \
 X(RsExcite, "rsExcite", "Excitation", Choice, 0, 1, 1, 0, "", "Pure|Enhanced", "Resonator", "Enhanced pre-emphasises and slightly drives the input so narrow sources excite more partials (brighter).") \
 X(ShRef, "shRef", "Shift Reference", Int, 24, 96, 48, 0, "note", "", "Shift", "MIDI note meaning 'no shift'. Shift = played note - reference (relative transposition, not absolute retuning).") \
 X(ShFormant, "shFormant", "Formant Preserve", Bool, 0, 1, 0, 0, "", "", "Shift", "Approximate formant preservation by 16-band envelope matching to the unshifted input.") \
 X(ShSpread, "shSpread", "Voice Spread", Float, 0, 100, 50, 0, "%", "", "Shift", "Pans shifted voices across the stereo field.") \
 X(ShLimit, "shLimit", "Interval Limit", Int, 0, 24, 24, 0, "st", "", "Shift", "Largest allowed shift up or down; larger intervals fold back by octaves.") \
 X(IntRoot, "intRoot", "Interval Root", Int, 24, 96, 48, 0, "note", "", "Intervals", "Stored root note for interval sets.") \
 X(IntRefSource, "intRefSource", "Interval Reference", Choice, 0, 1, 0, 0, "", "Stored Root|MIDI Root", "Intervals", "Stored Root uses the root above; MIDI Root uses the lowest held MIDI note.") \
 X(IntMode, "intMode", "Interval Mode", Choice, 0, 1, 0, 0, "", "Chromatic|Scale", "Intervals", "Chromatic: offsets are semitones. Scale: offsets are diatonic scale steps in the chosen key.") \
 X(IntKey, "intKey", "Key", Choice, 0, 11, 0, 0, "", PA_KEYS, "Intervals", "Key for Scale mode.") \
 X(IntScale, "intScale", "Scale", Choice, 0, 6, 1, 0, "", "Major|Natural Minor|Dorian|Mixolydian|Harmonic Minor|Major Pentatonic|Minor Pentatonic", "Intervals", "Scale for Scale mode.") \
 X(IntCount, "intCount", "Interval Count", Int, 1, 6, 3, 0, "", "", "Intervals", "How many offsets are used.") \
 X(Int1, "int1", "Offset 1", Int, -24, 24, 0, 0, "", "", "Intervals", "Semitones (Chromatic) or scale steps (Scale).") \
 X(Int2, "int2", "Offset 2", Int, -24, 24, 3, 0, "", "", "Intervals", "Semitones (Chromatic) or scale steps (Scale).") \
 X(Int3, "int3", "Offset 3", Int, -24, 24, 7, 0, "", "", "Intervals", "Semitones (Chromatic) or scale steps (Scale).") \
 X(Int4, "int4", "Offset 4", Int, -24, 24, 12, 0, "", "", "Intervals", "Semitones (Chromatic) or scale steps (Scale).") \
 X(Int5, "int5", "Offset 5", Int, -24, 24, 15, 0, "", "", "Intervals", "Semitones (Chromatic) or scale steps (Scale).") \
 X(Int6, "int6", "Offset 6", Int, -24, 24, 19, 0, "", "", "Intervals", "Semitones (Chromatic) or scale steps (Scale).") \
 X(ChRoot, "chRoot", "Chord Root", Choice, 0, 11, 0, 0, "", PA_KEYS, "Chord", "Root of the stored chord.") \
 X(ChOctave, "chOctave", "Chord Octave", Int, 1, 6, 3, 0, "", "", "Chord", "Octave of the chord root (3 = C3 = MIDI 48).") \
 X(ChQuality, "chQuality", "Chord Quality", Choice, 0, 8, 1, 0, "", "Major|Minor|Sus2|Sus4|Fifth|Maj7|Min7|Add9|Custom", "Chord", "Stored chord type. Custom uses the six custom pitches (edit them on the keyboard).") \
 X(ChInversion, "chInversion", "Inversion", Int, 0, 3, 0, 0, "", "", "Chord", "Moves the lowest notes up an octave.") \
 X(ChSpread, "chSpread", "Voicing", Choice, 0, 2, 0, 0, "", "Close|Open|Wide", "Chord", "Close, open (2nd note up an octave) or wide (alternate notes up an octave).") \
 X(ChCustom1, "chCustom1", "Custom 1", Int, -1, 127, 48, 0, "note", "", "Chord", "-1 = unused.") \
 X(ChCustom2, "chCustom2", "Custom 2", Int, -1, 127, 51, 0, "note", "", "Chord", "-1 = unused.") \
 X(ChCustom3, "chCustom3", "Custom 3", Int, -1, 127, 55, 0, "note", "", "Chord", "-1 = unused.") \
 X(ChCustom4, "chCustom4", "Custom 4", Int, -1, 127, -1, 0, "note", "", "Chord", "-1 = unused.") \
 X(ChCustom5, "chCustom5", "Custom 5", Int, -1, 127, -1, 0, "note", "", "Chord", "-1 = unused.") \
 X(ChCustom6, "chCustom6", "Custom 6", Int, -1, 127, -1, 0, "note", "", "Chord", "-1 = unused.") \
 X(ArpMode, "arpMode", "Arp Pattern", Choice, 0, 4, 0, 0, "", "Up|Down|Up/Down|As Played|Random", "Arp", "Order of arpeggiated notes. Random is deterministic (seeded).") \
 X(ArpSync, "arpSync", "Arp Sync", Bool, 0, 1, 1, 0, "", "", "Arp", "On: rate is a musical division of host (or standalone) tempo. Off: free rate in Hz.") \
 X(ArpRate, "arpRate", "Arp Rate", Choice, 0, 5, 1, 0, "", "1/4|1/8|1/8T|1/16|1/16T|1/32", "Arp", "Step length when synced.") \
 X(ArpFreeRate, "arpFreeRate", "Arp Free Rate", Float, 0.5f, 20, 4, 4, "Hz", "", "Arp", "Steps per second when not synced.") \
 X(ArpGate, "arpGate", "Arp Gate", Float, 5, 100, 70, 0, "%", "", "Arp", "Note length as a fraction of the step.") \
 X(ArpOctaves, "arpOctaves", "Arp Octaves", Int, 1, 3, 1, 0, "", "", "Arp", "Octave range of the pattern.") \
 X(ArpSwing, "arpSwing", "Arp Swing", Float, 0, 75, 0, 0, "%", "", "Arp", "Delays every second step.") \
 X(ArpVelVar, "arpVelVar", "Arp Velocity Variation", Float, 0, 100, 0, 0, "%", "", "Arp", "Deterministic per-step velocity variation.") \
 X(Tempo, "tempo", "Internal Tempo", Float, 40, 240, 120, 0, "BPM", "", "Timing", "Used for sync when the host provides no tempo or is stopped, and in the standalone app.") \
 X(BbdTime, "bbdTime", "BBD Time", Float, 30, 2000, 375, 400, "ms", "", "Delay", "Echo time when not synced.") \
 X(BbdSync, "bbdSync", "BBD Sync", Bool, 0, 1, 0, 0, "", "", "Delay", "Locks time to a tempo division.") \
 X(BbdDiv, "bbdDiv", "BBD Division", Choice, 0, 14, 8, 0, "", PA_DIVS, "Delay", "Synced echo time (T = triplet, D = dotted).") \
 X(BbdFeedback, "bbdFeedback", "BBD Feedback", Float, 0, 95, 42, 0, "%", "", "Delay", "Repeat amount. Loop filtering and bounded saturation keep it stable.") \
 X(BbdTone, "bbdTone", "BBD Tone", Float, 500, 16000, 3200, 3000, "Hz", "", "Delay", "Low-pass inside the feedback loop: each repeat gets darker.") \
 X(BbdAge, "bbdAge", "BBD Age", Float, 0, 100, 28, 0, "%", "", "Delay", "Coordinated darkening, saturation and companding colour. Adds no hiss by itself.") \
 X(BbdMotion, "bbdMotion", "BBD Motion", Float, 0, 100, 15, 0, "%", "", "Delay", "Wow, flutter and clock drift depth.") \
 X(BbdRate, "bbdRate", "BBD Motion Rate", Float, 0.02f, 8, 0.35f, 0.5f, "Hz", "", "Delay", "Wow rate (flutter runs faster).") \
 X(BbdStereo, "bbdStereo", "BBD Stereo", Choice, 0, 2, 0, 0, "", "Stereo|Ping-Pong|Mono Spread", "Delay", "Stereo: independent channels. Ping-Pong: alternating sides. Mono Spread: mono source, offset times.") \
 X(BbdTimeMode, "bbdTimeMode", "BBD Time Change", Choice, 0, 1, 0, 0, "", "Smooth|Tape", "Delay", "Smooth: crossfades to the new time. Tape: ramps the time continuously (pitch bends).") \
 X(BbdNoise, "bbdNoise", "BBD Noise", Float, 0, 100, 0, 0, "%", "", "Delay", "Quiet signal-dependent texture. Zero when nothing is playing.") \
 X(BbdLevel, "bbdLevel", "BBD Level", Float, -60, 6, -8, -12, "dB", "", "Delay", "BBD wet level.") \
 X(IvTime, "ivTime", "Interval Time", Float, 100, 4000, 500, 700, "ms", "", "Delay", "Echo time when not synced.") \
 X(IvSync, "ivSync", "Interval Sync", Bool, 0, 1, 0, 0, "", "", "Delay", "Locks time to a tempo division.") \
 X(IvDiv, "ivDiv", "Interval Division", Choice, 0, 14, 8, 0, "", PA_DIVS, "Delay", "Synced echo time.") \
 X(IvFeedback, "ivFeedback", "Interval Feedback", Float, 0, 90, 35, 0, "%", "", "Delay", "Repeat amount (internally limited further in Feedback Cascade).") \
 X(IvTaps, "ivTaps", "Tap Count", Int, 1, 3, 3, 0, "", "", "Delay", "Number of pitch-shaped taps.") \
 X(IvTap1Semi, "ivTap1Semi", "Tap 1 Interval", Int, -24, 24, 0, 0, "st", "", "Delay", "Tap transposition in semitones.") \
 X(IvTap2Semi, "ivTap2Semi", "Tap 2 Interval", Int, -24, 24, 7, 0, "st", "", "Delay", "Tap transposition in semitones.") \
 X(IvTap3Semi, "ivTap3Semi", "Tap 3 Interval", Int, -24, 24, 12, 0, "st", "", "Delay", "Tap transposition in semitones.") \
 X(IvTap1Level, "ivTap1Level", "Tap 1 Level", Float, -60, 0, 0, -12, "dB", "", "Delay", "Tap level.") \
 X(IvTap2Level, "ivTap2Level", "Tap 2 Level", Float, -60, 0, -6, -12, "dB", "", "Delay", "Tap level.") \
 X(IvTap3Level, "ivTap3Level", "Tap 3 Level", Float, -60, 0, -9, -12, "dB", "", "Delay", "Tap level.") \
 X(IvTap1Pan, "ivTap1Pan", "Tap 1 Pan", Float, -100, 100, -40, 0, "%", "", "Delay", "Tap pan.") \
 X(IvTap2Pan, "ivTap2Pan", "Tap 2 Pan", Float, -100, 100, 0, 0, "%", "", "Delay", "Tap pan.") \
 X(IvTap3Pan, "ivTap3Pan", "Tap 3 Pan", Float, -100, 100, 40, 0, "%", "", "Delay", "Tap pan.") \
 X(IvSmear, "ivSmear", "Smear", Float, 0, 100, 30, 0, "%", "", "Delay", "Grain jitter and short diffusion of the repeats (not a hidden reverb).") \
 X(IvGrain, "ivGrain", "Grain Length", Float, 20, 500, 100, 100, "ms", "", "Delay", "Pitch-shift window (Stable) or fragment length basis (Clock). Reverse needs longer grains.") \
 X(IvDirection, "ivDirection", "Direction", Choice, 0, 2, 0, 0, "", "Forward|Reverse|Alternating", "Delay", "Reverse plays captured past windows backwards (adds acquisition delay). Alternating: odd taps reverse.") \
 X(IvPitchMode, "ivPitchMode", "Pitch Behaviour", Choice, 0, 1, 0, 0, "", "Stable|Clock", "Delay", "Stable: tempo independent of interval. Clock: playback rate changes pitch AND fragment duration together.") \
 X(IvShiftPlace, "ivShiftPlace", "Shift Placement", Choice, 0, 1, 0, 0, "", "Output|Feedback Cascade", "Delay", "Output: each repeat is shifted once. Cascade: shifting happens inside the loop so repeats climb/descend (bounded).") \
 X(IvDriver, "ivDriver", "Pitch Driver", Choice, 0, 2, 0, 0, "", "Fixed Intervals|MIDI Relative|Arp Relative", "Delay", "Fixed: tap intervals only. MIDI/Arp Relative: taps are additionally transposed by (lowest note - reference).") \
 X(IvRef, "ivRef", "Interval Reference", Int, 24, 96, 48, 0, "note", "", "Delay", "Reference note for MIDI/Arp Relative (default C3).") \
 X(IvTone, "ivTone", "Interval Tone", Float, 1000, 20000, 9000, 5000, "Hz", "", "Delay", "Low-pass in the repeat loop.") \
 X(IvLevel, "ivLevel", "Interval Level", Float, -60, 6, -8, -12, "dB", "", "Delay", "Interval delay wet level.") \
 X(PlDecay, "plDecay", "Plate Decay", Float, 0.5f, 30, 8, 4, "s", "", "Reverb", "Approximate broadband RT60.") \
 X(PlPredelay, "plPredelay", "Plate Pre-delay", Float, 0, 250, 24, 50, "ms", "", "Reverb", "Gap before the plate starts.") \
 X(PlDiffusion, "plDiffusion", "Plate Diffusion", Float, 0, 100, 85, 0, "%", "", "Reverb", "Sparse attack (low) to dense velvet (high).") \
 X(PlTone, "plTone", "Plate Tone", Float, 1000, 20000, 6000, 5000, "Hz", "", "Reverb", "High-frequency damping in the tank.") \
 X(PlLowRatio, "plLowRatio", "Plate Low Decay", Float, 0.2f, 1.5f, 1.0f, 0, "x", "", "Reverb", "Low-frequency decay relative to the main decay.") \
 X(PlMotion, "plMotion", "Plate Motion", Float, 0, 100, 20, 0, "%", "", "Reverb", "Slow tank modulation depth.") \
 X(PlRate, "plRate", "Plate Motion Rate", Float, 0.01f, 2, 0.2f, 0.3f, "Hz", "", "Reverb", "Tank modulation rate.") \
 X(PlSize, "plSize", "Plate Size", Float, 50, 150, 100, 0, "%", "", "Reverb", "Scales tank delays (smoothly).") \
 X(PlLevel, "plLevel", "Plate Level", Float, -60, 6, -6, -12, "dB", "", "Reverb", "Plate wet level.") \
 X(WaDecay, "waDecay", "Wash Decay", Float, 4, 120, 24, 20, "s", "", "Reverb", "Approximate broadband RT60 of the wash network.") \
 X(WaBloom, "waBloom", "Wash Bloom", Float, 0, 100, 65, 0, "%", "", "Reverb", "How slowly energy builds: long diffusion and delayed injection into the network.") \
 X(WaSize, "waSize", "Wash Size", Float, 25, 200, 100, 0, "%", "", "Reverb", "Scales the network (smoothly).") \
 X(WaTone, "waTone", "Wash Tone", Float, 1000, 20000, 4800, 5000, "Hz", "", "Reverb", "High-frequency damping.") \
 X(WaLowRatio, "waLowRatio", "Wash Low Decay", Float, 0.2f, 1.5f, 0.8f, 0, "x", "", "Reverb", "Low-frequency decay relative to the main decay.") \
 X(WaMotion, "waMotion", "Wash Motion", Float, 0, 100, 30, 0, "%", "", "Reverb", "Decorrelated delay modulation depth.") \
 X(WaRate, "waRate", "Wash Motion Rate", Float, 0.005f, 1, 0.1f, 0.1f, "Hz", "", "Reverb", "Modulation rate.") \
 X(WaLevel, "waLevel", "Wash Level", Float, -60, 6, -6, -12, "dB", "", "Reverb", "Wash wet level.") \
 X(HarmEnable, "harmEnable", "Harmony Enable", Bool, 0, 1, 1, 0, "", "", "Harmony", "Turns the Classic harmony on or off. Off = ordinary ambience (the settings are kept).") \
 X(Mix, "mix", "Dry/Wet", Float, 0, 100, 50, 0, "%", "", "Mix", "Overall dry/wet blend. 50% = dry and wet both at their full levels; towards 0% the wet fades out (0% = dry only), towards 100% the dry fades out (100% = wet only).") \
 X(HaDecay, "haDecay", "Hall Decay", Float, 0.2f, 20, 2.6f, 2.5f, "s", "", "Reverb", "Approximate broadband RT60 of the hall.") \
 X(HaPredelay, "haPredelay", "Hall Pre-delay", Float, 0, 250, 18, 50, "ms", "", "Reverb", "Gap before the early reflections and the tail.") \
 X(HaSize, "haSize", "Hall Size", Float, 5, 100, 55, 0, "%", "", "Reverb", "Small room (low) to large hall (high): scales reflections and the tank (smoothly).") \
 X(HaTone, "haTone", "Hall Tone", Float, 1000, 20000, 7000, 5000, "Hz", "", "Reverb", "High-frequency damping: the decay at this frequency is half the set decay.") \
 X(HaEarly, "haEarly", "Hall Early Reflections", Float, 0, 100, 35, 0, "%", "", "Reverb", "Level of the early-reflection pattern (the room's first walls).") \
 X(HaDiffusion, "haDiffusion", "Hall Diffusion", Float, 0, 100, 70, 0, "%", "", "Reverb", "How quickly the tail becomes dense.") \
 X(HaLowRatio, "haLowRatio", "Hall Bass Multiplier", Float, 0.5f, 2.0f, 1.15f, 0, "x", "", "Reverb", "Low-frequency decay relative to the main decay (crossover about 350 Hz).") \
 X(HaMotion, "haMotion", "Hall Motion", Float, 0, 100, 25, 0, "%", "", "Reverb", "Random 'wander' of the tank delays: removes metallic ringing, adds gentle chorus.") \
 X(HaRate, "haRate", "Hall Motion Rate", Float, 0.05f, 3, 0.7f, 0.5f, "Hz", "", "Reverb", "Speed of the wander.") \
 X(HaLevel, "haLevel", "Hall Level", Float, -60, 6, -6, -12, "dB", "", "Reverb", "Hall wet level.") \
 X(Shimmer, "shimmer", "Shimmer", Float, 0, 100, 0, 0, "%", "", "Reverb", "Pitch-shifted feedback into the reverb: each pass rises by the shimmer interval (octave-up shimmer).") \
 X(ShimmerPitch, "shimmerPitch", "Shimmer Interval", Choice, 0, 4, 0, 0, "", "+12|+7|+19|+24|-12", "Reverb", "Interval of each shimmer pass, in semitones.") \
 X(TpTime, "tpTime", "Tape Time", Float, 40, 1000, 320, 300, "ms", "", "Delay", "Head 1 echo time when not synced; heads 2 and 3 are at 2x and 3x.") \
 X(TpSync, "tpSync", "Tape Sync", Bool, 0, 1, 0, 0, "", "", "Delay", "Locks head 1 to a tempo division (heads 2 and 3 follow at 2x and 3x).") \
 X(TpDiv, "tpDiv", "Tape Division", Choice, 0, 14, 5, 0, "", PA_DIVS, "Delay", "Synced head 1 time.") \
 X(TpFeedback, "tpFeedback", "Tape Feedback", Float, 0, 110, 45, 0, "%", "", "Delay", "Repeat intensity. Above about 100% the tape runs away into bounded self-oscillation.") \
 X(TpHeads, "tpHeads", "Tape Heads", Choice, 0, 6, 5, 0, "", "1|2|3|1+2|2+3|1+3|1+2+3", "Delay", "Which playback heads sound (and feed back): head 1 = Time, head 2 = 2x, head 3 = 3x.") \
 X(TpTone, "tpTone", "Tape Tone", Float, 1000, 12000, 4200, 4000, "Hz", "", "Delay", "Tape high-frequency loss: each repeat gets darker.") \
 X(TpDrive, "tpDrive", "Tape Drive", Float, 0, 100, 35, 0, "%", "", "Delay", "Record level into the tape: warmth, compression and saturation.") \
 X(TpWow, "tpWow", "Tape Wow & Flutter", Float, 0, 100, 25, 0, "%", "", "Delay", "Motor and capstan speed wobble (pitch drift of the repeats).") \
 X(TpSpread, "tpSpread", "Tape Spread", Float, 0, 100, 60, 0, "%", "", "Delay", "Places the active heads across the stereo field.") \
 X(TpHiss, "tpHiss", "Tape Hiss", Float, 0, 100, 0, 0, "%", "", "Delay", "Tape noise while the tape carries sound (fades out a few seconds after the input stops).") \
 X(TpLevel, "tpLevel", "Tape Level", Float, -60, 6, -8, -12, "dB", "", "Delay", "Tape wet level.")

enum ParamId : int
{
#define PA_ENUM(e, ...) e,
    PA_PARAMS (PA_ENUM)
#undef PA_ENUM
    kNumParams
};

constexpr int kParamVersion = 1;

const ParamInfo& paramInfo (int index) noexcept;
int paramIndexForId (const std::string& id) noexcept; // -1 if unknown
std::vector<std::string> paramChoices (int index);
/** Value <-> normalised [0,1] mapping (skewed when centre set). */
float paramToNormalised (int index, float value) noexcept;
float paramFromNormalised (int index, float norm) noexcept;
float paramSnap (int index, float value) noexcept; // clamps, rounds ints/choices/bools
std::string paramValueToText (int index, float value);
bool paramTextToValue (int index, const std::string& text, float& out);

/** Plain parameter snapshot read by the DSP engine. */
struct ParamSet
{
    std::array<float, kNumParams> v {};
    ParamSet();
    float operator[] (int i) const noexcept { return v[(size_t) i]; }
    float& operator[] (int i) noexcept { return v[(size_t) i]; }
    int i (int idx) const noexcept { return (int) (v[(size_t) idx] + 0.5f - (v[(size_t) idx] < 0 ? 1.0f : 0.0f)); }
    bool b (int idx) const noexcept { return v[(size_t) idx] >= 0.5f; }
};

/** Classic-only build: legacy harmony method Off -> Harmony Enable off; FFT/Resonator/Shift -> Classic.
    Applied whenever a saved state, preset or A/B slot is loaded. */
void migrateLegacyHarmony (ParamSet& ps) noexcept;
/** The harmony method the engine actually runs (Classic or Off). */
int effectiveHarmonyMethod (const ParamSet& ps) noexcept;
/** Dry/Wet mix law: 50% = both at unity (original behaviour), 0% dry only, 100% wet only. */
inline float mixDryGain (float mixPercent) noexcept { const float m = mixPercent / 100.0f; return m <= 0.5f ? 1.0f : std::max (0.0f, 2.0f * (1.0f - m)); }
inline float mixWetGain (float mixPercent) noexcept { const float m = mixPercent / 100.0f; return m >= 0.5f ? 1.0f : std::max (0.0f, 2.0f * m); }

// Delay division in beats for PA_DIVS index.
double divisionBeats (int index) noexcept;

} // namespace pa
