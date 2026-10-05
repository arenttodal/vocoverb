# Playable Ambience (working title) — vocoded delay & reverb

Play the harmony of the ambience while the original voice or instrument stays untouched. Sing a phrase, stop, and
change the chord of the remaining tail with MIDI or the on-screen keys.

Products (version 0.1.0, local evaluation builds):

| Product | Type | Notes |
|---|---|---|
| Playable Ambience.app | Standalone | Demo/File/Live sources, experiments, audio+MIDI settings, WAV export |
| Playable Ambience.vst3 | VST3 audio effect with MIDI input | Primary target for Ableton Live |
| Playable Ambience.component | AU music effect (`aumf PaAm Arnv`) | Logic: MIDI-controlled effect, audio via sidechain |
| Playable Ambience Audio.component | AU effect (`aufx PaAa Arnv`) | Logic: ordinary Audio FX, stored chord / intervals / keys |

## Coverage
| Area | Implementations |
|---|---|
| Delay | BBD character (smooth/tape time changes, age, wow/flutter, stereo/ping-pong/mono-spread); Interval (Stable and Clock, forward/reverse/alternating, output or bounded feedback cascade, up to 3 taps) |
| Reverb | Plate (Dattorro-topology tank); Wash (16-line modulated FDN, bloom); freeze for both |
| Harmony | Off, Classic filter-bank vocoder (24/32/48 bands), FFT/STFT vocoder, Tuned Resonator, Interval Shift |
| Placement / routing | After Space / Before Space; Parallel, Delay → Reverb, Reverb → Delay |
| Notes | MIDI + on-screen keys, stored chord (8 snapshots), interval bank (chromatic/scale), arpeggiator |
| Performance | Latch, sustain, note attack/release, transition (crossfade/glide), ducking, wet-only, panic, tail kill |
| Comparison | A/B with loudness match, 16 factory presets, repeatable experiments, offline audition export |

## Build
```
./scripts/bootstrap.sh                          # toolchain check + pinned JUCE into .deps/JUCE
./scripts/build-and-package-macos.sh universal  # macOS: build, test, auval/pluginval, sign ad-hoc, ZIP/DMG
./scripts/build-linux.sh && ./scripts/test.sh   # Linux development build + tests
```
See `docs/Quick-Start.md` for use, `docs/DECISIONS.md` for design decisions, `docs/BUILD_STATUS.md` for state,
and `THIRD-PARTY-NOTICES/` for licences.
