# Visual validation — GUI v2 against the approved reference

- Reference: `references/playable-ambience-approved-gui.png`, 1536 × 1024, SHA-256 `6a30d8e4…fbafe68`
  (manifest: `design/reference/measurements.json`). The approved image arrived as a WEBP and was stored losslessly
  as PNG, so its own compression noise (about ±2 levels in flat areas) is part of every difference below.
- Starting point: `first-build/first-build-initial-1280x928.png`, the previous build's native editor (standalone,
  default size), captured from commit `25a2b4b` before this revision.
- Captures: `capture/`, rendered by the native JUCE editor through `--selftest --screenshots` (Linux/X11 software
  renderer in this environment; macOS CI runs the same capture and comparison, see the job log
  "Visual comparison of the native macOS capture"). The canonical captures render the main view alone at scale 1
  (`reference-fixture-1536x1024.png`) and 2 (`reference-fixture-2x.png`). No host chrome, no masks.
- Comparison: `compare/` from `tools/visual/compare.py`: `*-side.png` (reference | capture), `*-overlay.png` (50 %),
  `*-diff.png` (absolute difference, auto-contrast), `metrics.json`.

## How the reference fixture works
The fixture sets the approved state on a disposable processor (Held in the Afterglow, A, BBD + SYNC 1/4, 42 %,
3.2 kHz, 28 %, −8.0 dB; Wash 24 s, 65 %, 4.8 kHz, 30 %, −6.0 dB; Parallel, After Space; Harmony on, Chord C minor
(C3 E♭3 G3 = MIDI 48/51/55), Hold Last, Depth 68 %, Colour 35 %, Transition 180 ms, Duck 24 %; Latch on; Dry/Wet
35 %), runs the processor so the real voices settle on the stored chord, then feeds envelopes extracted numerically
from the PNG (`tools/visual/extract_fixture.py`) through the production graph renderers. It is enabled only by the
self-test capture. The live captures (`capture/live-*.png`) use the real processor with deterministic demo audio.

## Results (canvas pixels; text boxes from `tools/visual/measure.py`)
| Check | Result |
|---|---|
| Principal rectangles, control centres, dividers, wells | Placed at the manifest coordinates (taken from the PNG); overlay shows edge agreement 0.84–0.98 per region |
| Text baselines / extents | 34 of 36 text boxes within 0–3 px of the reference (title, section titles, knob labels, readouts, buttons, strip labels). Exceptions below |
| Knobs (Feedback, Bloom, Depth, Dry/Wet crops) | Same face size (68 px, header 59 px), 9 tick dots at 30°, 270° sweep, marker r 0.40–0.86, fixed upper-left light. Edge agreement 0.85–0.96 |
| Delay waveform | Same baseline (y 277.5), first-burst location and height, repeat envelope, sparse tail; vertical micro-spikes with ivory cores and narrow orange halo. Column-luma mean abs diff ≈ 7 |
| Reverb cloud | Same centre line, orange-to-ivory progression, tapered tail, sub-pixel to 1.5 px points; reference texture is coarser ("squiggles") and its early core brighter. Column-luma mean abs diff ≈ 9 |
| Harmony ribbons | Same lanes (G, E♭, C at y 727.5 / 761 / 794.5), attack swell near x 132–143, decaying contour, thin warm core, hairy orange detail |
| Colours | Flat anchors within 0–5 levels per channel; the reference's lower-card / backdrop warmth is approximated by gradients |

Region metrics (mean absolute difference per RGB channel, 0–255; edge agreement = share of reference / capture edge
pixels with a counterpart within 1 px):

| Region | MAD R,G,B | ref edges matched | capture edges matched |
|---|---|---|---|
| full | 12.8, 12.0, 12.3 | – | – |
| header | 14.3, 14.1, 15.6 | 0.87 | 0.91 |
| delay card | 9.0, 7.9, 8.0 | 0.84 | 0.95 |
| reverb card | 15.2, 13.6, 12.7 | 0.88 | 0.94 |
| delay graph | 11.2, 7.7, 6.9 | 0.63 | 0.86 |
| reverb graph | 23.8, 19.2, 16.2 | 0.80 | 0.90 |
| routing strip | 8.7, 10.5, 11.8 | 0.95 | 0.96 |
| harmony header | 8.1, 9.8, 11.4 | 0.98 | 0.98 |
| harmony graph | 17.6, 10.9, 8.3 | 0.57 | 0.88 |
| harmony knobs | 10.3, 11.6, 12.7 | 0.84 | 0.83 |
| keyboard + performance | 21.2, 22.0, 23.5 | 0.69 | 0.80 |
| dropdown BBD | 6.3, 7.2, 9.3 | 1.00 | 1.00 |
| preset bar | 8.3, 9.9, 12.4 | 0.98 | 0.92 |

These numbers are supporting evidence only; the side-by-side images were inspected region by region during the
correction loop (graph colour/tone mapping, spike taper, cloud density, header knob, title weights, value fields,
pictogram sizes, axis label rows, placement split were all corrected from those inspections).

## Remaining discrepancies (specific)
| Region | Detected mismatch | Correction made | Remaining deviation |
|---|---|---|---|
| Reverb axis | Reference "8 s" label sits at x 1465, 31 px right of its own 1 s grid (other labels are on the grid) | Labels placed at their true ticks (78.1 px/s, grid every second) | "8 s" at x 1434 instead of 1465 (`compare/reverb-graph-side.png`) |
| Harmony axis | Reference labels are unevenly spaced (177/218/238/189 px) | Uniform 102.75 px/s between the reference's 0 s and 8 s positions | "2 s"/"4 s"/"6 s" up to 29 px from the raster positions |
| Keyboard | Reference black keys are not at consistent piano positions (raster artefact) and the white-key labels C4/C5 drift | True piano geometry, C2–C6, 29 white keys of 33.4 px; highlights from real voices | Black keys differ by up to ~10 px from the raster; largest MAD region (21–23) |
| IN/OUT meters | Reference shows orange segments at the top of dark bars at rest | Meters show real peak levels (silent in the fixture) | Fixture meters are dark |
| Reverb texture | Reference particles form short curly filaments; early core brighter | Stratified point field, hue-preserving tone map, density ∝ energy^1.5 | Finer, more uniform grain; MAD 16–24 in the well |
| Harmony note labels | Reference "E♭" uses a wide flat glyph | Inter's ♭ (U+266D) | Label 9 px narrower |
| Dry/Wet label | Reference label 58 px wide | Inter SemiBold cap 9.2 | 64 px wide, 2 px lower |
| Knob shading | Reference outline is heavier on the lower right and the face slightly greyer at the bottom | Outline gradient, bevel, shadow tuned from sampled radial profiles | Knob-crop MAD 11–17 (mostly label/readout anti-aliasing and shading) |
| Fonts | Reference glyphs are not Inter exactly | Inter 4.0 weights calibrated by cap height and tracking | Glyph shapes differ subtly in every text box |

## Proportions pass (after GUI v2)
Deliberate departures from the raster, requested after review: smaller brand and section titles, compact Dry/Wet,
preset bar and bottom buttons. Before / after crops: `proportions/` (top = before, bottom = after;
`full-overlay-before-after.png` is a 50 % blend). Measured on the native capture (canvas px):

| Item | Before | After |
|---|---|---|
| Title PLAYABLE (heavy) / AMBIENCE (regular) | cap 27 px (≈ 37 px em) | 28 px em, cap height 20 px |
| Subtitle | cap 9 px (≈ 12 px em) | 9.75 px em (cap 7 px), ~2.75 px tracking, 7 px below the title |
| DELAY / REVERB / HARMONY | cap 18 px (≈ 25 px em) | 20.5 px em, cap 15 px, identical for all three, centred on the header lines y 146 / 662 |
| Section dot | 17 px | 13 px visible, ~10 px gap to the title |
| Dry/Wet | 59 px face | 44 px face, 54 px tick ring; caption 10.5 px em, 4 px ring → label, 2 px label → value |
| Preset bar / A/B | 52 / 34 px high | 40 / 29 px |
| Utility icons (load, save, gear, panic, tail kill) | 27–31 px boxes | 18–21 px visible in 32 px hit areas |
| Bottom buttons | 2 × 2 incl. ADVANCED, 136–141 × 44–47 | Latch, Freeze 110 × 33; Wet Only 228 × 33 (full width); 8 px gaps; 11.75 px em medium labels |

Preserved: a pixel diff of the reference-fixture capture before and after shows **0 changed pixels** (> 8 levels) in
the delay, reverb and harmony graphs, all 14 macro knobs and the keyboard (routing strip: 4 anti-aliasing pixels).
The region metrics above therefore change only for the header, Dry/Wet, preset bar and performance crops, which
now differ from the raster on purpose (current `compare/metrics.json`: full 13.5 / 13.1 / 13.5, header 16.7 / 16.8 /
18.4, preset bar 13.4 / 15.4 / 18.2, Dry/Wet crop 23.0 / 26.2 / 28.8, keyboard + performance 21.9 / 25.5 / 27.8;
every graph and macro-knob region is unchanged from the table above).

## Contextual settings redesign (check report)
The Advanced overlay is retired; see `docs/GUI-Implementation-Note.md` for the parameter-location map and the
migration table. Captures (native editor, `capture/`): `detail-bbd`, `detail-interval-{taps,pitch,character}`,
`detail-plate`, `detail-wash`, `detail-harmony-{chord,intervals,arp,midi,voice,vocoder}`, `detail-closed-graphs-live`,
`settings-{midi,audio,support}`. BBD, Plate and Wash fit their graph rectangle (704 × 215) without scrolling;
the Harmony pages fit 912 × 160; every Settings page fits 470 × 480.

| # | Acceptance path | Result |
|---|---|---|
| 1 | Open Reverb settings, change Diffusion / Size | Sliders are the real `plDiffusion` / `plSize` parameters (host gestures, automation, presets) |
| 2 | Switch Plate ↔ Wash with Reverb settings open | View rebuilds for the new mode (self-test: PLATE → WASH SETTINGS) |
| 3 | Open Delay settings while Reverb settings are open | Reverb returns to its graph; one view at a time (self-test) |
| 4 | Sync and division | SYNC in the Delay settings heading; TIME macro shows the division when synced |
| 5 | BBD → Interval with Delay settings open | View rebuilds with TAPS / PITCH / CHARACTER (self-test) |
| 6 | Harmony chord / arp | Chord badge opens Harmony settings on the source page; Arp page follows FREE / SYNC (rate vs free rate) |
| 7 | Graphs live on return | Hidden graphs are not drawn; they rebuild from the live history on return (`detail-closed-graphs-live`) |
| 8 | Settings panel | Gear toggles it; pages MIDI / SETUP, AUDIO, SUPPORT; anchored under the gear, inside the editor, no scrim |
| 9 | Outside click / Escape | Outside click hits the catcher layer, not the Dry/Wet or Feedback knob below (self-test, parameter unchanged); Esc closes the panel, then an open effect view |
| 10 | Presets / A-B while open | A/B recall with Delay settings open follows the slot's mode (self-test); presets reload the same way |

Not verified here: pointer interaction on a real Mac or in a DAW (the checks above drive the same code paths
headlessly), VoiceOver reading order.

## Native macOS capture (CI run 37455117141, macos-15, universal build)
The same fixture captured by the macOS build (CoreGraphics renderer) and compared with the same tool:
full 12.9 / 12.1 / 12.4, delay graph 10.8 / 7.3 / 6.7, reverb graph 23.5 / 18.9 / 16.0, harmony graph 17.4 / 10.5 / 8.1,
knob Feedback 13.7 / 14.8 / 15.6 (edges 0.96), preset bar edges 0.99 — within ±1 of the Linux numbers above, so the
evidence images here represent the Mac editor. UI cost on the macOS runner: graph update + composite 1.3 ms at 1×,
4.1 ms at 2×; worst-case full-canvas repaint 20 ms at 1×, 69 ms at 2×.

## Functional checks performed for this revision
- Core tests 36/36 (new: Classic-only migration + Dry/Wet law; Dry/Wet endpoints 0 % = dry exactly, 100 % = Wet Only,
  50 % bit-identical to the previous blend; click-free switches with a steady final-state reference).
- Standalone self-test 37/37 including legacy sessions (Off → harmony off, FFT → Classic, missing mix → 50 %),
  A/B, presets, export, editor captures at default/minimum/large/2x, open-menu render. Now 53 checks including the
  contextual-settings checks listed above.
- Host MIDI check (VST3 on Linux here; VST3 + both AUs on macOS CI): MIDI still changes the output, same RMS as before.
- pluginval v1.0.4 strictness 8 on the Linux VST3 (editor included): SUCCESS. macOS auval/pluginval run in CI.
- UI cost (Linux software renderer): one graph update + repaint ≈ 1–4.5 ms at 1×, 6–11 ms at 2× when full of light;
  idle wells are not redrawn. Worst case full-canvas repaint (all controls + graphs): 34 ms at 1×, 125 ms at 2×
  (only happens on resize; normal repaints are limited to the changed regions).

## Not performed
- Visual check on a real Retina Mac display and inside Ableton Live / Logic (CI captures are headless).
- Pixel-identical output: not achievable against a generated raster (fonts, stochastic texture); deviations are quantified above.
