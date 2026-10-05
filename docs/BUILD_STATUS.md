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
- Linux CI: build, tests, self-test, pluginval v1.0.4 strictness 8 on the VST3: PASS.

## Next actions
- Confirm auval / pluginval results on macOS from CI run 2 (now printed into the job log).

## Known limitations
- Live/Logic host tests, hardware live input and listening tests are not possible in this environment.
- Ad-hoc signed local builds (no Developer ID / notarization credentials).
