# Playable Ambience 0.1.0 — Quick Start

Play the harmony of the ambience while your original audio stays untouched. Sing or play a phrase, stop, then
change the chord of the remaining tail with the keys.

## 1. Install and hear the first demo (3 steps)
1. Double-click **Install Playable Ambience.command** (per-user install, no admin rights). If macOS says the file
   cannot be opened, Control-click it and choose **Open**. These are ad-hoc signed local builds, not notarized.
2. Open **~/Applications/Playable Ambience.app**. If it is blocked, Control-click > Open, or System Settings >
   Privacy & Security > **Open Anyway** (this approves only this app).
3. Click **Audio / MIDI** to pick your output device, then press **▶** in the SOURCE strip. The demo source is a
   synthetic sung phrase followed by silence: listen to the tail after the phrase ends.

## 2. Change the harmony
- Click keys on the on-screen keyboard (or play a MIDI controller). With **Hold Last** (default) the last chord
  stays after you lift your hands. The keyboard and the chord readout show the notes actually voiced.
- Or choose **CHORD** in the Harmony card and pick root/quality, or **INTERVALS**, or **ARP**.
- No keys yet? A fresh preset with Hold Last uses its stored chord (C minor in most presets).

## 3. Compare
- Turn **DEPTH** up: 0 % = ordinary ambience, 100 % = only the harmonised wet.
- Switch the method: **CLASSIC** (filter-bank vocoder), **FFT** (STFT vocoder), **RESONATOR** (tuned resonances
  excited by the wet signal), **SHIFT** (relative transposition — not absolute retuning).
- Compare **AFTER SPACE** (the existing tail follows new chords) and **BEFORE SPACE** (chords are imprinted, then echo).
- Compare **PARALLEL**, **DELAY → REVERB**, **REVERB → DELAY**; BBD/INTERVAL; PLATE/WASH.
- Use **A / B** in the header; Advanced > Mix / Timing has Copy A>B and a bounded wet loudness match.
- The SOURCE menu has **Experiments** (e.g. *C minor > A-flat > F minor*, *Freeze / Revoice*) with fixed timing
  markers so comparisons are repeatable.

## 4. Freeze
Press **FREEZE** while the tail rings, stop the source, then change chords: the frozen texture is revoiced
(After Space). Freeze never makes sound on its own; LATCH (holds notes) and FREEZE (holds audio) are separate.

## 5. Files, presets, export
- **Load File...** (or drag a WAV/AIFF/FLAC onto the window; up to 2 minutes). Source level is separate from the mix.
- Save presets with the disk icon (user presets: ~/Library/Application Support/Playable Ambience/Presets).
- **Export WAV** renders the current source plus a 5–30 s tail offline with your settings and MIDI
  (experiment sequence, or MIDI you recorded with the ● button).
- **Save Experiment** writes preset + settings + recorded MIDI + diagnostics into a folder for your feedback.

## 6. Live input
Click **Live Input** (use headphones). macOS asks for microphone access the first time — only then. Choose the input
device and channels in **Audio / MIDI**, and enable your MIDI controller there.

## 7. Ableton Live (VST3)
1. Rescan plug-ins (Preferences > Plug-Ins). Insert **Playable Ambience** on an audio track or a Return track.
2. Create a MIDI track, set **MIDI To** = the track hosting Playable Ambience / "Playable Ambience", monitor **In**
   (or arm it). Choose **MIDI** as note source.
3. On a Return track (or whenever the source track stays audible) enable **WET ONLY**.
4. Plugin default timing is **Studio** (dry and wet aligned, latency reported). Timing/quality changes wait for the
   transport to stop.

## 8. Logic Pro (AU)
- **MIDI control:** create a Software Instrument track, in the Instrument slot choose **AU MIDI-controlled Effects >
  ARN > Playable Ambience**, then pick your audio track or bus in the plug-in header's **Side Chain** menu. Play
  that instrument track's MIDI. Enable **WET ONLY** if the source track stays audible.
- **No MIDI needed:** insert **Playable Ambience Audio** as a normal Audio FX and use CHORD, INTERVALS (stored root),
  ARP over the stored chord, or the on-screen keys. It shows "MIDI n/a" because this AU type receives no MIDI.

## 9. Troubleshooting
- **Silent harmony?** The carrier never sounds without audio: you need input audio or a ringing/frozen tail.
  Check the IN meter, the source strip, and the chord readout (NO NOTES = check the no-note policy or press keys).
- **MIDI not arriving?** Watch the MIDI dot in the Harmony card (MIDI source). In Live, check MIDI To and monitoring;
  in Logic use the MIDI-controlled AU. Advanced > MIDI shows the channel filter.
- **Stuck notes:** the **!** button (Panic) releases all notes; the wave-cross button (Tail Kill) also clears tails.
- **Plugin missing:** run the installer, then rescan (Live) or restart Logic; check Logic's Plug-in Manager.
- **CPU / clicks:** Advanced > Mix / Timing > Quality (Eco), larger buffer in Audio / MIDI; Advanced > Diagnostics
  shows processing time.
- **Blocked by Gatekeeper:** see step 1–2; nothing here requires disabling Gatekeeper.
