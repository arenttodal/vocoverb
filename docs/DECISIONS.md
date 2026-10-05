# Engineering decisions (Playable Ambience 0.1.0)

Routine decisions made autonomously during the unattended build, with reasons.

## Platform and dependencies
- **Empty repository** → C++20, CMake, JUCE. JUCE pinned to **8.0.15** (commit 91ad83ae…), the latest 8.x at
  build time; 9.0.x was available but newer than the API surface used here. Pins in `DEPENDENCIES.lock`.
- **DSP core has no JUCE dependency** (`Source/Core`, `Source/DSP`, `Source/Presets/FactoryPresets.*`): one engine
  shared by VST3, both AUs, the standalone, the offline renderer, tests and the benchmark.
- **FFT**: project-owned radix-2 FFT (contiguous per-stage twiddles) instead of juce::dsp, so the core stays
  JUCE-free and testable in seconds.
- **Signalsmith Stretch not used**: the spec permits a dual-window delay-line shifter. An original one is used for both
  Interval Shift and Interval Delay; artefacts (grain-rate comb lines) and latency (W/2) are measured in tests.
- **Font**: Inter 4.1 (SIL OFL) embedded so screenshots on Linux and the Mac build look the same. The mockup's
  typeface was not identified; Inter is a clean neutral sans with similar proportions.
- **JUCE licence**: no commercial configuration found; evaluation build prepared under the AGPLv3 option with full
  source (see THIRD-PARTY-NOTICES/NOTICES.md).

## Identifiers
- Manufacturer code `Arnv`, company "ARN"; MIDI-controlled AU `aumf PaAm Arnv` (com.arn.playableambience);
  Audio AU companion `aufx PaAa Arnv` (com.arn.playableambience.audioau). VST3 and standalone share
  com.arn.playableambience (JUCE uses one bundle id per plugin target). Version 0.1.0.
- Parameter IDs are versioned `juce::ParameterID{id, 1}` from a single table (`Source/Core/Params.h`); IDs never
  depend on GUI labels.

## Audio graph semantics
- Dry path: only Dry Level and Studio alignment delay. Header OUTPUT knob = Wet Level (labelled and described).
- After Space: `wet = (1-d)·align(S(x)) + d·H(S(x))`; Before Space: `wet = S((1-d)·align(x) + d·H(x))`; linear Depth.
- Parallel After Space uses two independent harmony units (one per branch, shared notes); series routes harmonise the
  combined wet once; Before Space harmonises the input once (second unit only aligns the excluded branch).
- Harmony Off = identity path (Depth stored but ignored).
- Mode changes: the outgoing engine keeps running with no input and fades over 1.5 s (tail preserved) or 30 ms
  (Clear Tail on Change), then is cleared progressively (bounded memset per block). Routing/placement/timing
  changes use a 25 ms wet dip; method changes crossfade 50 ms with two method states at most.
- Quality changes go through the tail-kill path (fade, progressive clear, fade in) because FFT size / Wash line
  count change state layout; documented in the Quality help text.

## Timing / latency
- Live: dry immediate, wet carries the active method's buffering (FFT N + N/4, Shift W/2+2). Studio: every harmony
  path is padded to the quality's maximum (FFT N + N/4), dry delayed by the same amount, reported to the host.
- **FFT latency is N + N/4, not N**: frame work (2 forward FFTs, envelopes, 1 inverse) is spread over the following
  hop so a 64-sample block never carries a whole frame (p95 went from 105 % to 8 % of the deadline for 4096-point
  frames). Measured by the WOLA identity test.
- In hosts, Studio-affecting changes (timing, quality) wait until the transport stops; the UI shows a message.
- Tail length: VST3 reports an infinite tail (never suspend a frozen wash); AU reports 130 s (finite, valid).

## Notes and MIDI
- Up to 6 voices; steal quiet releasing voice, else oldest, with 8 ms fade. Effective voiced notes are what the UI shows.
- Hold Last keeps notes released within 120 ms of the final key-up (a lifted chord, not just the last note).
- Latch replacement uses the Chord Window (default 30 ms) after all keys are up.
- All Notes Off releases that origin's notes and clears latch/hold, but does not cancel a fresh preset's stored-chord
  fallback; Panic/Tail Kill do (they mean "no notes").
- Program Change 0–7 recalls chord snapshots (applied on the message thread ~30 ms later).
- Interval Delay Pitch Driver default Fixed (avoids double pitch motion); MIDI Relative uses (lowest note − reference).

## Processing details (initial calibrations)
- Classic: 24/32/48 log bands 90 Hz–12 kHz, 4th-order (Eco: 2nd-order) band-pass, carrier band normalisation
  bounded at +18 dB, calibrated to about −6 dB vs a broadband modulator.
- FFT: sqrt-Hann WOLA, hop N/4, envelope smoothing 1/12–2/3 octave, carrier envelope ≥ 1/2 octave and ≥ 1.2×
  lowest f0, ratio bounded at 40, measured level about −3 dB vs modulator.
- Resonator: complex one-pole (rotational) resonators — energy-safe under retuning; partial caps 6/12/16.
- Shift: windows 40/60/80 ms (Eco/Std/High); identity path at unity ratio; optional 16-band formant matching.
- Plate: Dattorro topology at 29 761 Hz scaled to the session rate, stereo pre-delay/diffusers, low-band decay split.
- Wash: 16 (Eco 8) modulated lines 30–152 ms × Size, Hadamard feedback, 4 input + 4 bloom all-passes, bloom also
  weights injection towards longer lines.
- Freeze (reverbs): loop gain → T60 10 000 s, damping open, input faded to the overdub level, energy guard (bleeds
  gain if mean-square exceeds 1.4× the level captured 0.4–0.5 s after engaging). Delay freeze: crossfaded loop
  (≈1 s rounded to delay repeats, 80 ms crossfade) of the recorded delay output.
- Wet protection limiter at about −1 dBFS on the wet branch only, with an activity indicator.

## Standalone
- Custom JUCE application (not the generic wrapper): opens audio with **zero input channels** so no microphone
  prompt appears at launch; Live Input requests permission and enables input channels only when clicked.
- Device choice is stored by name (JUCE device XML); MIDI inputs by identifier.
- `--selftest [--screenshots DIR] [--report FILE]` runs the real processor/editor headless for CI.
