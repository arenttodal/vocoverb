# Playable Ambience 0.1.0 — Quick Start

Play the harmony of the ambience while your original audio stays untouched. Sing or play a phrase, stop, then
change the chord of the remaining tail with the keys.

## 1. Install and hear the first demo (3 steps)
1. Double-click **Install Playable Ambience.command** (per-user install, no admin rights). If macOS says the file
   cannot be opened, Control-click it and choose **Open**. These are ad-hoc signed local builds, not notarized.
2. Open **~/Applications/Playable Ambience.app**. If it is blocked, Control-click > Open, or System Settings >
   Privacy & Security > **Open Anyway** (this approves only this app).
3. Click **Audio / MIDI** to pick your output device, then press **PLAY DEMO** in the SOURCE strip. The demo source is a
   synthetic sung phrase followed by silence: listen to the tail after the phrase ends.

## 2. Change the harmony
- Click keys on the on-screen keyboard (or play a MIDI controller). With **Hold Last** (default) the last chord
  stays after you lift your hands. The keyboard and the chord readout show the notes actually voiced.
- Or pick **Chord**, **Intervals** or **Arp** in the Harmony card's **SOURCE** dropdown. Click the chord badge
  (e.g. **C MINOR**) to open Harmony settings on the source page: the stored chord (root, quality, octave, inversion,
  voicing, 8 snapshots), the interval set or the arpeggiator. The three small buttons next to the dropdown choose what happens with no keys held:
  **pin** = Hold Last, **wave** = Release, **cloud** = Ambient.
- No keys yet? A fresh preset with Hold Last uses its stored chord (C minor in most presets).

## 3. Compare
- Turn **DEPTH** up: 0 % = ordinary ambience, 100 % = only the harmonised wet.
- Turn harmony off and on with the orange dot next to **HARMONY** (off = ordinary ambience). The harmony is the
  Classic filter-bank vocoder; its voices, bands and envelopes are in **Harmony settings** (the sliders icon at the
  right of the Harmony header). Each card has the same icon: the settings replace that card's graph until you click
  **GRAPH**, the icon again, or press Esc. Only one card shows its settings at a time; the sound never pauses.
- **DRY / WET** (top right) blends the untouched source against the ambience: 50 % keeps both at full level,
  0 % = dry only, 100 % = wet only. The ambience output level and the dry level are in the gear: **Settings > Audio**.
- Compare **AFTER SPACE** (the existing tail follows new chords) and **BEFORE SPACE** (chords are imprinted, then echo).
- Compare the routing pictograms (**parallel**, **D → R**, **R → D**) and the BBD/Interval and Plate/Wash dropdowns.
- The displays show the last few seconds since a phrase started (0 s = the onset); when everything is silent they
  show a dim preview of the current delay pattern and decay.
- Use **A / B** in the header; **Settings > Audio** has Copy A>B, a bounded wet loudness match (MATCH B) and whether
  A/B switching clears the tail.
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

## 7. Ableton Live — use the VST3
In Live use **Playable Ambience** from **Plug-ins > VST3 > ARN**. Do not use the Audio Units versions in Live:
Live does not deliver MIDI to AU effects, and **Playable Ambience Audio** has no MIDI input at all (it is for Logic
without MIDI). If you only see Audio Units entries, turn on **Settings > Plug-Ins > Use VST3 Plug-in System Folders**
and click **Rescan**.

1. **Audio track** (your voice, instrument or recorded clip): drag **Playable Ambience** (VST3) onto it, *after*
   any instrument if it is a MIDI-instrument track. (On a **Return** track it also works; then enable **WET ONLY**.)
2. **MIDI track** (new, empty, no instrument): this is where you play or draw the chords.
   - **MIDI From**: your keyboard (or *All Ins*).
   - **MIDI To**: top menu = the audio/return track from step 1; **bottom menu = "Playable Ambience"**.
     If the bottom menu shows only *Track In* or nothing, Live did not load the VST3 — re-insert it from the VST3 folder.
   - **Monitor = In** (or arm the track). Live only forwards live keyboard notes from a monitored/armed track;
     notes in MIDI clips on this track are sent while the clip plays.
3. In the plug-in's **HARMONY** card choose **MIDI** as note source. The line next to the MIDI dot shows
   *"MIDI IN ch 1 last C3 (n notes)"* as soon as notes arrive. If it says *"No MIDI yet"*, the routing in step 2 is
   not reaching the plug-in; if the status strip says *"MIDI notes arriving but ignored: Note Source is CHORD"*,
   switch the note source to MIDI (the four *Matched* comparison presets use a stored chord on purpose).

The plug-in's default timing is **Studio** (dry and wet aligned, latency reported to Live). Timing/quality changes
wait for the transport to stop. Without any MIDI, a fresh preset plays its stored chord (C minor) until the first note.

## 8. Logic Pro (AU)
- **MIDI control:** create a Software Instrument track, in the Instrument slot choose **AU MIDI-controlled Effects >
  ARN > Playable Ambience**, then pick your audio track or bus in the plug-in header's **Side Chain** menu. Play
  that instrument track's MIDI. Enable **WET ONLY** if the source track stays audible.
- **No MIDI needed:** insert **Playable Ambience Audio** as a normal Audio FX and use CHORD, INTERVALS (stored root),
  ARP over the stored chord, or the on-screen keys. It shows "MIDI n/a" because this AU type receives no MIDI.

## 9. Troubleshooting
- **Silent harmony?** The carrier never sounds without audio: you need input audio or a ringing/frozen tail.
  Check the IN meter, the source strip, and the chord readout (NO NOTES = check the no-note policy or press keys).
- **MIDI not arriving?** **Settings > MIDI / Setup** (and the SOURCE dropdown tooltip) tells you what the plug-in
  receives (*No MIDI yet* / *MIDI in: ch .., last ..* / *dropped by the channel filter*). In Live use the **VST3** and pick
  *Playable Ambience* in the MIDI track's lower **MIDI To** menu with Monitor = In (section 7); in Logic use the
  MIDI-controlled AU with Side Chain (section 8). Settings > MIDI / Setup has the channel filter (Omni by default).
- **Stuck notes:** the **!** button (Panic) releases all notes; the wave-cross button (Tail Kill) also clears tails.
- **Plugin missing:** run the installer, then rescan (Live) or restart Logic; check Logic's Plug-in Manager.
- **CPU / clicks:** Settings > Audio > Quality (Eco), larger buffer in Audio / MIDI; Settings > Support shows
  processing time.
- **Blocked by Gatekeeper:** see step 1–2; nothing here requires disabling Gatekeeper.
