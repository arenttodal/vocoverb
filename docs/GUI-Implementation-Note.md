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
| Harmony method tabs Off/Classic/FFT/Resonator/Shift | Removed. Harmony on/off = orange dot next to HARMONY (`harmEnable`). Classic settings: Harmony settings |
| Note source tabs MIDI/Chord/Intervals/Arp | SOURCE dropdown (same `noteSource` enum values) |
| Hold Last / Release / Ambient segmented | Pin / wave / cloud radio pictograms (`noNotePolicy` 0/1/2) |
| Source bar (chord, snapshots, intervals, arp) | Chord badge (e.g. C MINOR) opens Harmony settings on the source page for the current source |
| MIDI activity text | SOURCE dropdown tooltip, Harmony settings (MIDI source) and Settings > MIDI / Setup (live status; the Audio AU says it receives no MIDI); transient warnings inside the harmony well |
| Routing text segments | Three pictograms: parallel branch, D → R, R → D (`routing` 0/1/2) |
| Delay BBD/Interval and Reverb Plate/Wash segments | Dropdowns (`delayMode`, `reverbMode`) |
| Header OUTPUT knob (`wetLevel`) | Header DRY / WET knob (`mix`). Output (wet level) and Dry level: gear > Settings > Audio |
| Latency / AU strings in the routing strip | Settings > Audio (latency) and Settings > Support (diagnostics) |
| Kebab (⋮) menus and the ADVANCED button opening the full-editor Advanced overlay | Retired. Sliders icon in each card header (contextual effect settings) and the gear (Settings panel); see below |
| Mode-dependent knob sets (Interval: Time/Feedback/Interval/Smear/Level; Plate: Decay/Pre-delay/Tone/Motion/Level) | Unchanged |

## Contextual effect settings and the Settings panel (replaces the Advanced overlay)
- **Effect settings** (`Source/UI/SettingsViews.cpp`, `EffectDetail`): the sliders icon at the right of the Delay,
  Reverb and Harmony headers (tooltips "Delay settings", "Reverb settings", "Harmony settings"; accent tint while
  open) swaps that card's graph rectangle for a warm-ivory inset (9 px corners, 14 px padding, 30 px heading row,
  44–46 px rows, 8 px row gap; 2 columns for Delay / Reverb, 3–4 for the wider Harmony well). Nonmodal: the macro
  knobs, mode dropdown and everything else stay live. Only one card shows its settings at a time; **GRAPH** (thin
  graph icon), the sliders icon again, or Esc return to the graph. Esc is not taken while a text field or menu is
  active (they consume it first).
- Content follows the current mode only (BBD *or* Interval, Plate *or* Wash, the current note source); a mode change
  while open rebuilds the view. Values of the inactive mode stay stored and are not shown. Front-panel macros are not
  repeated. Delay keeps **SYNC** in the settings heading (the graph's SYNC button is hidden with the graph).
- Local pages: Interval TAPS / PITCH / CHARACTER; Harmony source page (CHORD, INTERVALS, ARP or MIDI) / VOICE /
  VOCODER. BBD, Plate and Wash are one page each and fit without scrolling. A **?** toggle shows a one-sentence help
  line for the control under the pointer; every control also has a one-sentence tooltip.
- Hidden graphs are not drawn while their settings are shown; audio, telemetry and the capture clock keep running,
  and the graph rebuilds from the live history rings the moment it returns (self-test capture
  `detail-closed-graphs-live`). No fade is used (the swap is instant), so captures and screen readers never see a
  half-transparent state.
- **Settings panel** (`SettingsPanel`, 470 × 480 canvas px): the header gear toggles it, anchored under the gear and
  clamped inside the canvas, with no scrim. Pages MIDI / SETUP, AUDIO, SUPPORT (the last page used is remembered in
  the session). Closes with its × button, Esc, or a click outside: a transparent layer under the panel catches that
  click, so it only closes the panel and never reaches the control underneath (self-test hit-test check). The Audio AU
  shows the truthful "receives no MIDI" status and no channel selector; the standalone adds AUDIO / MIDI DEVICES....
- Code-level routes: `PluginEditor::showEffectSettings`, `showSettings`, `showDestination` (old Advanced tab names
  "Mix / Timing" → Settings > Audio, "Diagnostics" → Settings > Support, "MIDI" → Settings > MIDI / Setup,
  "Harmony" / "Delay" / "Reverb" → that card's settings). `AdvancedPanel` and the chord call-out were removed.

### Parameter location map (every parameter and action; ids, enums and ranges unchanged)
| Destination | Parameters / actions |
|---|---|
| Header | preset browser, prev / next, favourite, load, save, A/B, IN/OUT meters, `mix` (DRY / WET), gear |
| Card headers | `delayEnable`, `reverbEnable`, `harmEnable` (dots); `delayMode`, `reverbMode` (dropdowns); chord badge; `noteSource`; `noNotePolicy` (pictograms) |
| Delay macros | BBD: `bbdTime` / `bbdDiv` (by sync), `bbdFeedback`, `bbdTone`, `bbdAge`, `bbdLevel`; Interval: `ivTime` / `ivDiv`, `ivFeedback`, `ivTap2Semi` (INTERVAL), `ivSmear`, `ivLevel` |
| Delay settings, BBD | `bbdSync` (heading), `bbdMotion`, `bbdStereo`, `bbdRate`, `bbdTimeMode`, `bbdNoise`, `clearTailOnChange` |
| Delay settings, Interval | heading `ivSync`; TAPS `ivTap1Semi`, `ivTap3Semi`, `ivTap1..3Level`, `ivTap1..3Pan` (tap 2 interval = INTERVAL macro); PITCH `ivPitchMode`, `ivDirection`, `ivShiftPlace`, `ivGrain`, `ivDriver`, `ivRef`; CHARACTER `ivTaps`, `ivTone`, `clearTailOnChange` |
| Reverb macros | Plate: `plDecay`, `plPredelay`, `plTone`, `plMotion`, `plLevel`; Wash: `waDecay`, `waBloom`, `waTone`, `waMotion`, `waLevel` |
| Reverb settings, Plate | `width`, `plSize`, `plDiffusion`, `plLowRatio`, `plRate`, `clearTailOnChange` |
| Reverb settings, Wash | `width`, `waSize`, `waLowRatio`, `waRate`, `clearTailOnChange` |
| Routing strip | `routing`, `placement` |
| Harmony macros | `depth`, `colour`, `transition`, `duckAmount` |
| Harmony settings, source page | Chord: `chRoot`, `chQuality`, `chOctave`, `chInversion`, `chSpread`, snapshots 1–8 + STORE; Intervals: `intRoot`, `intRefSource`, `intMode`, `intCount`, `intKey`, `intScale`, `int1..6`; Arp: `arpMode`, `arpSync`, `arpRate` / `arpFreeRate` (by sync), `arpOctaves`, `arpGate`, `arpSwing`, `arpVelVar`; MIDI: `chordWindow`, `pitchBendRange`, `modWheelTarget`, live MIDI status |
| Harmony settings, VOICE | `polyphony`, `noteAttack`, `noteRelease`, `transitionMode`, `applyHarmonyTo` (parallel routing), `clCarrier`, `clDetune`, `clStereoLink` |
| Harmony settings, VOCODER | `clBands`, `clAttack`, `clRelease`, `clFormant`, `clNoise` |
| On-screen keyboard | notes; `chCustom1..6` (Custom chord quality: click keys to toggle); pitch / mod wheels |
| Performance | `latch`, `freeze`, `wetOnly`, Panic, Tail Kill |
| Settings > MIDI / Setup | `midiChannel` (not in the Audio AU), `inputSource`, `refTuning`, `tempo`, `pitchBendRange`, `modWheelTarget`, live MIDI / device status, Audio / MIDI devices (standalone), Panic, Tail Kill |
| Settings > Audio | `wetLevel`, `dryLevel`, `timing`, `quality`, `wetLowCut`, `wetHighCut`, `duckAttack`, `duckRelease`, `serialSend` (series routing), `freezeTarget`, `freezeOverdub`, `wetTrim`, latency, Copy A>B, Copy B>A, Match B loudness, A/B clears tail |
| Settings > Support | read-only diagnostics, Copy diagnostics, Export..., Save Experiment... |
| No UI (sessions / automation only) | `harmMethod` (Classic-only migration); `fftSmooth`, `fftPersist`, `fftFormant`, `fftStereoLink`, `rsHarmonics`, `rsDecay`, `rsDamping`, `rsSpread`, `rsExcite`, `shRef`, `shFormant`, `shSpread`, `shLimit` (legacy methods, kept so old sessions load; not offered) |

### Migration from the Advanced tabs
| Advanced tab | Item | New home |
|---|---|---|
| Harmony | Harmony enable / Placement / Depth, Colour, Transition, Duck | Header dot / routing strip / Harmony macros |
| Harmony | Apply harmony to, Polyphony, Note attack / release, Transition mode, Carrier, Detune, Stereo link | Harmony settings > VOICE |
| Harmony | Bands, Envelope attack / release, Formant, Sibilance | Harmony settings > VOCODER |
| Harmony | No-note policy, Latch | Harmony header pictograms, LATCH |
| Delay | Delay enable / mode | Delay header dot / dropdown |
| Delay | BBD and Interval secondary parameters | Delay settings (BBD page; Interval TAPS / PITCH / CHARACTER) |
| Delay | Clear tail on change | Delay / Reverb settings "Tail on mode change: Let ring / Clear" |
| Delay | Freeze target, Freeze overdub | Settings > Audio |
| Reverb | Reverb enable / mode, Width, Plate / Wash secondary parameters | Reverb header / Reverb settings |
| MIDI | Note source, chord, intervals, arp, snapshots | SOURCE dropdown, Harmony settings source page (chord badge) |
| MIDI | Custom chord notes 1–6 | On-screen keyboard (Custom quality) |
| MIDI | MIDI channel, Reference tuning, Internal tempo, Bend range, Mod wheel, Panic, Tail kill | Settings > MIDI / Setup (bend / mod wheel also on the MIDI source page; Panic / Tail Kill also in the performance row) |
| MIDI | Chord window | Harmony settings > MIDI source page |
| Mix / Timing | Mix | Header DRY / WET |
| Mix / Timing | Wet / Dry level, Input source, Timing, Quality, latency, cuts, duck attack / release, series send, wet trim, A/B copy / match / clear tail | Settings > Audio (input source: Settings > MIDI / Setup) |
| Mix / Timing | Routing, Width, Freeze | Routing strip, Reverb settings, FREEZE |
| Diagnostics | Diagnostics text, Export, Save Experiment | Settings > Support |

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
