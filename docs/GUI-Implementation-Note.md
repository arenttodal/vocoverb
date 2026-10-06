# GUI v2 — implementation note

Target: `references/playable-ambience-approved-gui.png` (1536 × 1024, SHA-256
`6a30d8e49b1807262d4eb45971ba589e6433cd988292d6c3a749a9c78fbafe68`). Measurements: `design/reference/measurements.json`.
Evidence: `design/visual-validation/`.

## Canvas and scaling
- The main view is laid out once in canonical 1536 × 1024 coordinates (`Source/UI/PluginEditor.cpp`, `MainView::resized`).
  The whole canvas is scaled by one affine transform `s = window width / 1536` with a fixed aspect ratio
  (default 0.85, range 0.62 to 1.5). No reflow; knobs and labels never resize independently.
- Standalone: the source/transport strip (82 canvas px) sits above the canvas inside the same scaled container.
- Comparison captures render the main view alone at scale 1 (1536 × 1024) and 2 (Retina backing), without host chrome.

## Where the moved and compacted controls went
| Before | Now |
|---|---|
| Harmony method tabs Off/Classic/FFT/Resonator/Shift | Removed. Harmony on/off = orange dot next to HARMONY (`harmEnable`). Classic settings: Harmony ⋮ (Advanced > Harmony) |
| Note source tabs MIDI/Chord/Intervals/Arp | SOURCE dropdown (same `noteSource` enum values) |
| Hold Last / Release / Ambient segmented | Pin / wave / cloud radio pictograms (`noNotePolicy` 0/1/2) |
| Source bar (chord root/quality/octave/inversion/voicing, 8 snapshots + STORE, interval root/ref/mode/key/scale/count, arp pattern/rate/sync/octaves/gate) | Chord badge (e.g. C MINOR) opens a call-out with the controls for the current source; everything also stays in Advanced > MIDI |
| MIDI activity text | SOURCE dropdown tooltip (live MIDI status, Audio AU explains that it receives no MIDI); transient warnings appear top-right inside the harmony well |
| Routing text segments | Three pictograms: parallel branch, D → R, R → D (`routing` 0/1/2) |
| Delay BBD/Interval and Reverb Plate/Wash segments | Dropdowns (`delayMode`, `reverbMode`) |
| Interval Stable/Clock and Fwd/Rev/Alt (in the delay well) | Delay ⋮ (Advanced > Delay) |
| Header OUTPUT knob (`wetLevel`) | Header DRY / WET knob (new `mix`). Wet Level (output) and Dry Level: gear → Settings > Mix / Timing |
| Latency / AU strings in the routing strip | Settings > Mix / Timing and Diagnostics |
| Mode-dependent knob sets (Interval: Time/Feedback/Interval/Smear/Level; Plate: Decay/Pre-delay/Tone/Motion/Level) | Unchanged |

## Classic-only migration (`Source/Core/Params.cpp`)
- `harmMethod` keeps its id and enum slots (Off|Classic|FFT|Resonator|Shift) for saved sessions and host automation.
- `migrateLegacyHarmony()` runs for every loaded session (`stateFromXml`), preset, factory preset and A/B slot
  (`PluginProcessor::applyParams`): legacy **Off → `harmEnable` = 0** (method stored as Classic);
  **FFT / Resonator / Shift → Classic**.
- The engine runs `effectiveHarmonyMethod()` = Classic when `harmEnable` is on and `harmMethod` ≠ Off, else Off.
  An automation lane that still writes `harmMethod` = Off bypasses harmony; any other value runs Classic. No
  invisible FFT/Resonator/Shift path runs. The Classic DSP is unchanged (host MIDI check renders a bit-identical RMS
  0.115588 before and after this revision).
- The three FFT/Resonator/Shift comparison presets were replaced by Classic variants (48 bands, Bright, Hollow).
- Studio latency is unchanged (still sized for the largest legacy method) so existing sessions keep their alignment.

## Dry / Wet and output compatibility
- New parameter `mix` (id `mix`, 0..100 %, default 50 %, appended at the end of the table so no existing id or
  order changes). Law: dry gain = min(1, 2·(1 − m)), wet gain = min(1, 2·m). 50 % = both at unity, exactly the
  previous blend, so sessions saved before this parameter existed (no `mix` value) sound identical; 0 % = dry
  path only; 100 % = wet path only. Applied inside the existing smoothed dry/wet gains (no double mixing).
- `wetLevel` (output) and `dryLevel` keep their ids and meaning; they moved to Settings > Mix / Timing.
- WET ONLY overrides the blend (dry removed, wet at full level) and keeps the stored mix; the knob is dimmed and
  its tooltip says so. Turning WET ONLY off restores the blend.
- Tests: `params: Classic-only migration and Dry/Wet law`, `graph: Dry/Wet endpoints ...`, self-test "legacy session".

## Graph time contract and data feeds
- X axis = seconds since the capture started: an input onset after ≥ 0.25 s of silence, wet sound appearing with
  no capture running, or every ~9 s of continuous sound. Live energy fades within ~1.2 s of wet silence.
- Idle (no wet energy): Delay and Reverb show a dim preview derived from the current parameters (delay time / sync,
  feedback, tone, level; decay, bloom). Harmony shows faint baselines for voiced notes only.
- Feeds (audio thread → UI, relaxed atomics, fixed 4096-bucket rings of 256 samples, no locks or allocation):
  signed min/max of the delay and reverb stage outputs; per-voice min/max of the Classic synthesis band nearest each
  voice's fundamental (scaled by current Depth and duck) with the voice's note per bucket. Taps never change the output.
- Rendering: one additive light canvas per well (device resolution, quarter-resolution halo, hue-preserving tone
  map), redrawn only when new data arrives. Measured on the Linux software renderer: ~1.5 ms per graph at 1×,
  ~5–9 ms at 2× when the well is full of light (self-test log "graphs draw / render / blit").
- Reference visual fixture (visual-test only, `PluginEditor::setReferenceFixture`): numeric envelopes extracted
  from the PNG by `tools/visual/extract_fixture.py` (`Source/UI/ReferenceFixtureData.h`) drive the same renderers.
  It is enabled only by the self-test capture and never in normal operation.

## Fonts and assets
- Inter (SIL OFL 1.1): Regular, Medium, SemiBold, Bold (4.1) plus ExtraBold and Light added from the official v4.0
  release (`Resources/Fonts`, licence in `Resources/Fonts/Inter-OFL-LICENSE.txt`), embedded as binary data.
- No bitmaps are used for controls: knobs (one `KnobRenderer`, cached stationary layers per size and device scale,
  procedural arc/marker), icons and pictograms are vector paths.

## Fixes made along the way
- Topology switches (routing / placement) now also fade the signal entering the spaces and reset the harmony
  envelopes at the swap; previously a discontinuity could be recorded into a delay tail and replay one repeat later
  (found because the old click test had masked it with the FFT method).
