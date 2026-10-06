# Playable Ambience 0.2.0 — release notes

## What the classics do, and what this release takes from them
| Reference idea | Where it came from (public descriptions) | What 0.2 does with it (original implementation) |
|---|---|---|
| Halls built from a recirculating ring of all-pass / delay sections with slowly *randomised* delay lengths, early reflections, bass multiplier and crossover | Lexicon hall/random-hall family (Griesinger's designs as described in the literature; Dattorro 1997 describes the figure-eight relative) | New **Hall** mode: 12-tap early-reflection pattern, input diffusion, 4-section ring with smoothed-random + slow-sine "wander", 350 Hz bass split with a Bass Multiplier, decay-consistent damping; Size spans small room to large hall |
| Pitch-shifted energy recirculating inside a reverb ("shimmer") | Eventide (H-series pitch + reverb patches, ShimmerVerb), later Valhalla and others | New **Shimmer** for every reverb: +12 / +7 / +19 / +24 / −12, band-limited and level-guarded feedback |
| Tape loop with several playback heads at fixed spacing, head-selector combinations, intensity past self-oscillation, wow/flutter, head bump and tape saturation | Roland Space Echo / Echoplex style tape echoes | New **Tape** delay mode: 3 heads at 1x/2x/3x, 7 combinations, bounded runaway above 100 %, speed-proportional wow & flutter, motor glide, head bump, gap loss, optional programme-following hiss |
| Delays that duck under the performance and bloom in the gaps | TC 2290 "dynamic delay" | Already present (Duck); now showcased in the **Ducked Quarter Echo** preset |
| Frequency-dependent decay that stays consistent across delay lengths | Jot's absorbent-filter FDN design | Applied to all reverbs (see fixes) |

No code, coefficients or presets were copied from any product; trade names describe inspiration only.

## Flaws found (measured with the new `pa_spacelab` tool) and fixed
| Flaw | Measurement before | After |
|---|---|---|
| Wash decay shorter than the knob and Tone coupled to line length / Size: the per-line low-pass ran on 30–150 ms lines, so high frequencies died after a few hundred passes | Wash 24 s: T30 19.1 s, 1 kHz 15.1 s, 4 kHz 4.8 s; Wash 6 s: 1 kHz 4.9 s (size 100) vs 3.9 s (size 25) | Wash 24 s: 1 kHz 22.3 s; Wash 6 s: 1 kHz 5.8 s at size 25 and 5.7 s at size 100 (Tone no longer depends on Size) |
| Plate Tone not consistent with Decay (short plates were undamped, long plates dark) | Plate 1.5 s: 4 kHz 1.50 s (same as 1 kHz); Plate 8 s: 4 kHz 5.7 s | Decay at the Tone frequency is now always half the set decay (default 8 s plate unchanged: 4 kHz 5.7 s) |
| Delay time glided from a stale value after load / reset (BBD in Tape time mode from 375 ms, Interval from 500 ms) | first repeats of a fresh instance at the wrong time, with a pitch bend | starts exactly at the set time (new tape-head test) |
| Reverb choice was limited to plate and very long wash (Wash minimum 4 s) | no rooms, no natural halls | Hall: 0.2–20 s, room to hall |
| Factory presets all centred on the harmony feature; nothing ready for common mixing jobs | — | 14 new presets in Vocal, Guitar, Keys, Drums, Ambient, Mix and Showcase |

## New presets
Vocal Plate, Slapback Room, Ducked Quarter Echo, Space Echo Dub, Runaway Tape, Concert Hall, Drum Room, Snare Plate,
Shimmer Cathedral, Octave Abyss, Shimmer Choir, Tape into Hall, Synth Pad Bloom, Vocoded Tape Echo (30 factory presets
in total, all checked by the self-test to load and render finite, non-silent audio). Five new audio examples
(16–20) are rendered by `pa_render`.

## Verification
- Core tests 39/39 (new: decay accuracy of Hall / Plate / Wash at 1 kHz within ±15 % — measured 0.56/0.6, 2.60/2.6,
  6.21/6, 3.07/3, 5.82/6, 5.67/6 s; shimmer at 100 % for every interval decays to silence, Tape at 110 % feedback
  bounded and really self-oscillating; tape heads at exactly 1x/2x/3x). The 60 s stress test and the allocation-
  counting test now cycle through all three delay and reverb modes, shimmer and tape runaway.
- Standalone self-test passes, with new captures `detail-tape`, `detail-hall`, `detail-reverb-shimmer`.
- CPU (Linux CI-class machine, 64-sample blocks at 48 kHz): Tape (3 heads) + Hall 4.3 % of the deadline, Hall +
  Shimmer + Classic 13 %, new worst case (Tape + Wash + Shimmer + 2x Classic 48 High) 20 %, the same as the previous
  worst case.
- `pa_spacelab` output is written to `out/reports/spacelab.txt` by `scripts/test.sh`.

## Compatibility
See `docs/GUI-Implementation-Note.md` ("0.2.0: new modes and compatibility"): sessions and presets load unchanged;
host automation lanes recorded on the Delay/Reverb mode selectors in 0.1 should be re-recorded; Wash sounds brighter
and longer at the same Tone value because its decay now follows the knob.

## Not verified
Listening tests on studio monitors and inside Ableton Live / Logic were not possible in this environment; all claims
above are measurements. The new modes have not been compared side by side with the hardware or software that
inspired them.
