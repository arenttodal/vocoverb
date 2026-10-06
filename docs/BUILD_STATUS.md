# Build status

## Environment
- Development container: Linux x86_64 (no macOS). Native Mac binaries are built by GitHub Actions
  (`.github/workflows/build.yml`, `macos-15` runner, Xcode toolchain, universal arm64 + x86_64) on every push to the
  working branch; artifacts: `playable-ambience-macos` (ZIP, DMG, checksums) and `playable-ambience-macos-reports`.
- Mac one-command build on your own machine: `./scripts/build-and-package-macos.sh universal` (or `arm64`).

## Completed (verified)
- Core DSP engine, all modes; 34 headless tests passing on Linux and macOS (incl. no-allocation and sample-rate
  transition tests); AddressSanitizer + UBSan clean.
- macOS CI run 1 (commit 6f5c870): universal build of Playable Ambience.app / .vst3 / .component (aumf) /
  Audio.component (aufx), core tests PASS, standalone self-test PASS, packaging OK (ZIP 59 MB, DMG 64 MB, source
  ZIP), self-test of the app extracted from the ZIP PASS. Validation step reported 4 failures caused by the
  validation script's universal-binary `otool` parsing (fixed in 5987d7f).
- macOS CI run 2 (commit 5987d7f, run https://github.com/arenttodal/vocoverb/actions/runs/37382773602): all green.
  auval -strict aumf PaAm Arnv and aufx PaAa Arnv: AU VALIDATION SUCCEEDED. pluginval v1.0.4 strictness 8: SUCCESS
  for VST3, MIDI AU and Audio AU (both AUs log the warning "Disabling non-main buses failed"; the optional sidechain
  bus is kept by the host). Installer / reinstall-with-backup / uninstaller test against a throwaway HOME: PASS.
  Architectures: arm64 + x86_64; no non-system libraries linked.
- Linux CI: build, tests, self-test, pluginval v1.0.4 strictness 8 on the VST3, ASan core tests: PASS.
  Locally also pluginval strictness 10 (Linux VST3): SUCCESS.

- CI run 4 (commit dd482f7, run https://github.com/arenttodal/vocoverb/actions/runs/37436534447): host-side MIDI
  delivery check (pa_hostcheck) PASS on macOS for the VST3 and the MIDI AU (MIDI changes 63 % of the output) and
  confirms the Audio AU has no MIDI input; Linux VST3 PASS. All other checks green as in run 2.

## Next actions
- Host tests in Ableton Live and Logic Pro, hardware live input and listening feedback (see Feedback-Template).

## Known limitations
- Live/Logic host tests, hardware live input and listening tests are not possible in this environment.
- Ad-hoc signed local builds (no Developer ID / notarization credentials).
