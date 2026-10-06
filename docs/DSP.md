# DSP report — Playable Ambience 0.2.0

All processing is original C++ in `Source/Core` and `Source/DSP`, shared by every product. Numbers below are from
the automated tests (`Tests/`) at 48 kHz unless stated; see the Validation Report for the measured values of a
specific build.

## Signal graph
- Input is split immediately. **Dry** = input × Dry Level (× 0 when Wet Only), delayed only in Studio timing.
- **After Space**: `wet = (1−d)·align(S(x)) + d·H(S(x))`. **Before Space**: `wet = S((1−d)·align(x) + d·H(x))`.
  `d` = Harmony Depth (linear, with mod-wheel and Ambient-policy factors).
- **Parallel**: delay and reverb hear the same input; in After Space each branch has its own harmony state (shared
  notes); *Apply Harmony To* can exclude a branch. **Delay → Reverb / Reverb → Delay**: the first engine's wet
  output (× Serial Send) feeds the second; the first engine's own level is a separate direct tap; harmony
  processes the combined wet once. Disabled engines fall back to a valid graph (series bypass crossfades).
- Wet post chain: dry-envelope duck (up to −24 dB), wet low/high cut, M/S width, Wet Level × Wet Trim, protection
  limiter (≈ −1 dBFS, wet only), non-finite guard (resets the wet state, never the dry).

## Harmony methods
| Method | Algorithm | Latency |
|---|---|---|
| Classic | 24/32/48 log-spaced bands 90 Hz–12 kHz; 4th-order (Eco 2nd-order) RBJ band-pass on modulator L/R and carrier; rectified attack/release envelopes (default 12/220 ms); carrier band normalised to the voice gain (bounded +18 dB) so the modulator, not the carrier's tilt, shapes the spectrum; Colour = ±3 dB/oct tilt; Formant Shift moves envelopes across bands; optional >4 kHz noise preservation | 0 |
| FFT | STFT/WOLA, N = 1024/2048/4096 (×2 above 70 kHz), hop N/4, sqrt-Hann analysis + synthesis (sum = 2, normalised); L/R packed in one complex FFT, carrier in another, one inverse FFT for both outputs. Modulator envelope = RMS over ±(1/24…1/3) octave, carrier envelope ≥ ½ octave and ≥ 1.2 × lowest f0; output bin = carrier bin × env_mod/env_car (bounded 40) × tilt × voice gain; carrier phase kept. Frame work spread over the next hop. | **N + N/4** (2560 samples at Standard, 48 kHz) — measured by the identity-reconstruction test (error < −130 dB) |
| Resonator | Per voice up to 6/12/16 partials (Eco/Std/High) of complex one-pole resonators `z ← r·e^{jω}·z + b·x` (rotation form: retuning keeps the stored energy, so glides do not burst); partial decay T/(1+4·damping·k/P); excitation = wet signal × voice envelope (Enhanced: pre-emphasis + soft drive); partials above 0.45·fs dropped; steals fade 8 ms and clear state | 0 |
| Shift | Dual-window delay-line shifter per voice (windows 40/60/80 ms), sin² crossfades, true identity path at unity ratio; shift = note − Shift Reference, folded by octaves to the Interval Limit; optional 16-band formant matching to the unshifted input | 2 + W/2 (1442 samples at Standard, 48 kHz) |

Studio timing pads every harmony path to the quality's largest latency (the FFT value) so switching methods never
moves the wet timing; Live timing uses the active method's own latency.

## Carrier
Six voices, PolyBLEP saw (Soft), saw + 22 % pulse (Bright) or square (Hollow), optional second detuned oscillator,
one-pole tilt from Colour. Voice envelopes (attack, release; replacement uses the Transition time), optional glide,
pitch bend, and chord-size energy normalisation `1/√max(1, Σ(env·vel)²)`. The carrier is only ever heard through a
method's synthesis path: with no input or tail the output is numerically silent (tested in all routings).

## Delays
- **BBD**: cubic-interpolated stereo lines (≤ 2 s), Smooth (35 ms dual-head crossfade) or Tape (0.25 s slew) time
  changes, 2-pole loop low-pass (Tone × (1−0.45·Age)), 45 Hz DC blocking, tanh saturation (drive 1+2.5·Age),
  companding colour (write compressor / read expander with mismatched times), wow + flutter + drift, Stereo /
  Ping-Pong / Mono Spread, optional signal-dependent noise (silent without signal). Max-feedback test: bounded.
- **Tape** (new in 0.2): one tape loop (3.3 s) with three playback heads at 1×, 2× and 3× the head-1 Time
  (head spacing of the classic multi-head echoes), seven head combinations, feedback from the active heads normalised
  by √heads, record saturation with unity small-signal gain (Drive only adds compression and even/odd harmonics,
  never loop gain), per-head playback head bump (+2 dB shelf at 110 Hz) and two-stage gap loss (Tone), wow, flutter
  and drift applied as a tape-speed deviation (excursion grows with head distance), motor glide (0.32 s) on time
  changes, optional hiss that follows the programme and fades ~3 s after it stops. Feedback above ≈100 % runs away
  into bounded self-oscillation (tested at 110 %, all heads, Drive 100: peak 0.32, finite).
- **Interval**: main line + per-tap anti-aliased copies (4th-order low-pass at 0.45·fs/ratio for upward rates).
  Stable = two-grain delay-line shifter centred on the echo time; Reverse = backward grains (acquisition delay
  ≈ (1+ratio)·grain); Clock = fragments of 2·grain·(1+smear) read at the tap rate, so pitch and duration change
  together (reads never pass the write head). Output placement shifts each repeat once; Feedback Cascade shifts
  inside the loop (feedback capped at 0.765, filtered, saturated). Pitch Driver adds (lowest note − reference).

## Reverbs
- **Plate**: Dattorro figure-eight tank (reference lengths at 29 761 Hz, scaled to fs × Size 50–150 %), separate L/R
  pre-delay and input diffusers, modulated decay-diffusion all-passes, damping low-pass, low band split at 250 Hz
  for the Low Decay ratio, 14 output taps. Broadband loop gain from RT60 over the full loop.
- **Wash**: 16-line (Eco 8) FDN, lines 30–152 ms × Size 25–200 %, normalised Hadamard feedback, per-line damping and
  low split, per-line slow LFO modulation (Motion up to 6 ms), 4 input + 4 long bloom all-passes and
  bloom-weighted injection favouring long lines (slower energy build-up). Measured: Wash onset energy far below the
  plate's (≈ −17 dB vs +10 dB early/late ratio), ≈ −9.6 dB per 2.5 s at 24 s decay.
- **Hall** (new in 0.2): pre-delay, 12-tap stereo early-reflection pattern (4–84 ms × size, alternating signs,
  some cross-feed, one smoothing all-pass), 4+4 input diffusers, then a ring of four sections (modulated all-pass →
  delay → decay-consistent damping and 350 Hz bass split → all-pass → delay). Delay lengths are mutually incommensurate
  (largest loop ≈ 0.94 s at Size 100 %, ≈ 0.1 s at 5 %); the first all-pass of each section wanders by up to 1.1 ms
  with smoothed random plus a slow sine (the classic hall "random"/chorus idea), 7 output taps per channel from all
  sections. Measured at 1 kHz: 0.6 s/size 10 → 0.56 s, 2.6 s/size 55 → 2.60 s, 6 s/size 100 → 6.2 s; echo density
  0.9 reached in 45–95 ms.
- **Shimmer** (new in 0.2, all reverbs): the reverb output is pitch-shifted (two-grain, 71/83 ms windows, sin² cross-
  fade), band-limited 260 Hz – 8.5 kHz and fed back into the reverb input one chunk later, so every pass rises by the
  interval (+12, +7, +19, +24 or −12). The feed is level-guarded (≤ −10 dBFS) and the band limits remove energy after a
  few passes: at 100 % with a 4 s hall every interval decays to digital silence (tested).
- **Decay-consistent damping** (0.2 fix, all reverbs): each feedback segment's damping low-pass is solved so that the
  decay at the Tone frequency is a fixed fraction of the set decay (Plate and Hall ½, Wash 1/2.5) for any segment
  length, Size or Decay (Jot-style absorbent filters). Before this, Wash's per-line low-pass ran on 30–150 ms lines,
  so at the default 4.8 kHz Tone a 24 s Wash measured 15 s at 1 kHz and 4.8 s at 4 kHz, and Tone changed with Size.
  Now: 6 s Wash measures 5.8 s (size 25) and 5.7 s (size 100) at 1 kHz.
- **Freeze**: loop gains → RT60 10 000 s, damping open, input fades to Overdub, energy guard; tested over 60 s
  (bounded, finite; frozen silence stays silent). Delay freeze loops a crossfaded capture of the delay output.

## Real-time safety
No allocation, locks, logging or I/O on the audio path (an allocation-counting test exercises every mode switch,
freeze, panic and tail kill); progressive (bounded) clearing of large buffers; FTZ/DAZ denormal handling;
chunked processing (≤ 64 samples) with sample-accurate MIDI.
